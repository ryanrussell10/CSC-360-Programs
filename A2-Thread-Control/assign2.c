/*
 * assign2.c
 *
 * Name: Ryan Russell
 * Student Number: V00873387
 * Date: July 9th, 2020
 * CSC 360 Assignment 2
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> 
#include <pthread.h>
#include "train.h"

/*
 * If you uncomment the following line, some debugging
 * output will be produced.
 *
 * Be sure to comment this line out again before you submit 
 */

/* #define DEBUG	1 */

void ArriveBridge (TrainInfo *train);
void CrossBridge (TrainInfo *train);
void LeaveBridge (TrainInfo *train);

pthread_mutex_t waiting_lock;
pthread_mutex_t bridge_lock;
pthread_cond_t convar = PTHREAD_COND_INITIALIZER;
int east_crossed = 0;
int east_waiting = 0;
int west_waiting = 0;
int west_prev = -1;
int east_prev = -1;
int cur_west = 0;
int cur_east = 0;


/*
 * This function is started for each thread created by the
 * main thread.  Each thread is given a TrainInfo structure
 * that specifies information about the train the individual 
 * thread is supposed to simulate.
 */
void * Train ( void *arguments )
{
	TrainInfo	*train = (TrainInfo *)arguments;

	/* Sleep to simulate different arrival times */
	usleep (train->length*SLEEP_MULTIPLE);

	ArriveBridge (train);
	CrossBridge  (train);
	LeaveBridge  (train); 

	/* I decided that the paramter structure would be malloc'd 
	 * in the main thread, but the individual threads are responsible
	 * for freeing the memory.
	 *
	 * This way I didn't have to keep an array of parameter pointers
	 * in the main thread.
	 */
	free (train);
	return NULL;
}

/*
 * You will need to add code to this function to ensure that
 * the trains cross the bridge in the correct order.
 */
void ArriveBridge ( TrainInfo *train )
{

	printf ("Train %2d arrives going %s\n", train->trainId, 
			(train->direction == DIRECTION_WEST ? "West" : "East"));
	
	
	// Lock the train waiting area so only one train can record its info.
	pthread_mutex_lock(&waiting_lock);

	if (train->direction == 2) {
		east_waiting++;
		train->arrival = cur_east++;
	} else if (train->direction == 1) {
		west_waiting++;
		train->arrival = cur_west++;
	}	
	
	// Unlock the train waiting area.
	pthread_mutex_unlock(&waiting_lock);

	// Lock the main bridge..
	pthread_mutex_lock(&bridge_lock);
	
	if (train->direction == 2) {

		// Ensure that eastbound trains wait when two eastbound trains have crossed and a westbound train is waiting.
		while ((east_crossed >= 2 && west_waiting > 0) || (train->arrival != east_prev+1)) {
			//printf("Train %d is waiting.\n", train->trainId);
			pthread_cond_wait(&convar, &bridge_lock);
		}
	} else if (train->direction == 1) {
		
		// Ensure that westbound trains wait when less than two eastbound trains have crossed and there is one waiting.
		while ((east_crossed < 2 && east_waiting > 0) || (train->arrival != west_prev+1)) {
			//printf("Train %d is waiting.\n", train->trainId);
			pthread_cond_wait(&convar, &bridge_lock);
		}
	}		
}

/*
 * Simulate crossing the bridge.  You shouldn't have to change this
 * function.
 */
void CrossBridge ( TrainInfo *train )
{

	printf ("Train %2d is ON the bridge (%s)\n", train->trainId,
			(train->direction == DIRECTION_WEST ? "West" : "East"));
	fflush(stdout);
	
	/* 
	 * This sleep statement simulates the time it takes to 
	 * cross the bridge.  Longer trains take more time.
	 */
	usleep (train->length*SLEEP_MULTIPLE);

	printf ("Train %2d is OFF the bridge(%s)\n", train->trainId, 
			(train->direction == DIRECTION_WEST ? "West" : "East"));
	fflush(stdout);
}

/*
 * Add code here to make the bridge available to waiting
 * trains...
 */
void LeaveBridge ( TrainInfo *train )
{

	// Decrement the waiting line and record the train's arrival value.
	if (train->direction == 1) {
                west_waiting--;
		west_prev = train->arrival;
	} else if (train->direction == 2) {
  		east_waiting--;
		east_prev = train->arrival;
		
		// Reset the east_crossed counter so a waiting westbound train can cross.
		if (west_waiting > 0)
			east_crossed++;
	}	

	if (train->direction == 1 && east_crossed > 0)
		east_crossed = 0;
	
	// Release all waiting trains to check if it is their turn.
	pthread_cond_broadcast(&convar);

	// Unlock the main bridge.
	pthread_mutex_unlock(&bridge_lock);
}

int main ( int argc, char *argv[] )
{
	int		trainCount = 0;
	char 		*filename = NULL;
	pthread_t	*tids;
	int		i;

		
	/* Parse the arguments */
	if ( argc < 2 )
	{
		printf ("Usage: part1 n {filename}\n\t\tn is number of trains\n");
		printf ("\t\tfilename is input file to use (optional)\n");
		exit(0);
	}
	
	if ( argc >= 2 )
	{
		trainCount = atoi(argv[1]);
	}
	if ( argc == 3 )
	{
		filename = argv[2];
	}	
	
	initTrain(filename);

	if (pthread_mutex_init(&waiting_lock, NULL) != 0) {
		fprintf(stderr, "Mutex initialization failed.\n");
		exit(EXIT_FAILURE);
	}

	if (pthread_mutex_init(&bridge_lock, NULL) != 0) {
		fprintf(stderr, "Mutex initialization failed.\n");
		exit(EXIT_FAILURE);	
	}
	
	/*
	 * Since the number of trains to simulate is specified on the command
	 * line, we need to malloc space to store the thread ids of each train
	 * thread.
	 */
	tids = (pthread_t *) malloc(sizeof(pthread_t)*trainCount);
	
	/*
	 * Create all the train threads pass them the information about
	 * length and direction as a TrainInfo structure
	 */
	for (i=0;i<trainCount;i++)
	{
		TrainInfo *info = createTrain();
		
		printf ("Train %2d headed %s length is %d\n", info->trainId,
			(info->direction == DIRECTION_WEST ? "West" : "East"),
			info->length );

		if ( pthread_create (&tids[i],0, Train, (void *)info) != 0 )
		{
			printf ("Failed creation of Train.\n");
			exit(0);
		}
	}
	
	if (filename != NULL)
		initComplete();

	/*
	 * This code waits for all train threads to terminate
	 */
	for (i=0;i<trainCount;i++)
	{
		pthread_join (tids[i], NULL);
	}
	
	pthread_mutex_destroy(&waiting_lock);
	pthread_mutex_destroy(&bridge_lock);
	pthread_cond_destroy(&convar);

	free(tids);
	return 0;
}

