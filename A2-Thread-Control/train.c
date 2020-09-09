/*
 * train.c
 */
 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "train.h"
 
/* A global to assign IDs to our trains */ 
int idNumber = 0;

/* If this value is set to 1, trains lengths
 * etc will be generated randomly.
 * 
 * If it is set to 0, the lengths etc will be
 * input from a file.
 */
int doRandom = 0;

/* The file to input train data from */
FILE *inputFile;

/* You can assume that no more than 80 characters
 * will be on any line in the input file
 */
#define MAXLINE		80

void	initTrain ( char *filename )
{
	doRandom = 0;
	
	/* If no filename is specified, generate randomly */
	if ( !filename )
	{
		printf("No input file provided. Using random input.\n");
		doRandom = 1;
		srandom(getpid());
	}
	else
	{
		inputFile = fopen(filename, "r");
		printf("Opened input file.\n");
		
		if (inputFile == NULL) {
			fprintf(stderr, "Error opening file.");
			exit(EXIT_FAILURE);
		}
	}	
}

/*
 * Closes the input file once all input has been read.
 */
void initComplete() 
{
	fclose(inputFile);
	printf("Closed input file.\n\n");
}

 
/*
 * Allocate a new train structure with a new trainId, trainIds are
 * assigned consecutively, starting at 0
 *
 * Either randomly create the train structures or read them from a file
 *
 * This function malloc's space for the TrainInfo structure.  
 * The caller is responsible for freeing it.
 */
TrainInfo *createTrain ( void )
{
	TrainInfo *info = (TrainInfo *)malloc(sizeof(TrainInfo));
	char line[MAXLINE];

	/* I'm assigning the random values here in case
	 * there is a problem with the input file.  Then
	 * at least we know all the fields are initialized.
	 */	 
	info->trainId = idNumber++;
	info->arrival = 0;
	info->direction = (random() % 2 + 1);
	info->length = (random() % MAX_LENGTH) + MIN_LENGTH;
	
	// If an input file is provided, parse the current line.
	if (!doRandom)
	{
		fgets(line, MAXLINE, inputFile);
		
		// Set direction of train according to input.
		if ((line[0] == 'w') || (line[0] == 'W')) {
			info->direction = 1;
		} else if ((line[0] == 'e') || (line[0] == 'E')) {
			info->direction = 2;
		}
		
		// Ensure line can be converted to an int.
		int i = 0;
		for (i = 0; i < MAXLINE; i++) {	
			line[i] = line[i+1];
		}
		
		info->length = atoi(line);
	}
	return info;
}


