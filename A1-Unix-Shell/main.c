/*
 * main.c
 *
 * Ryan Russell
 * V00873387
 * May 26th, 2020
 * CSC 360 Assignment 1
 *
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <limits.h>
#include <signal.h>


/* Helper function to parse and retrieve the user command. */
char **get_cmd(char *cmd) {
	
	int i = 0;
	char *result;
	char **command = malloc(16 * sizeof(char*));
	
	if (command == NULL) {
		fprintf(stderr, "Malloc failure.\n");
		exit(EXIT_FAILURE);
	}

	result = strtok(cmd, " ");

	// Set each index of the command to the specified argument from the input.
	while (result != NULL) {
		command[i++] = result;

		// Check if strtok has returned NULL and indicates the end of the input.
		result = strtok(NULL, " ");	
	}

	command[i] = NULL;
	return command;
}


int main (int argc, char * argv[])
{
	pid_t child; // The pid of the child process created by fork.
	int status; // Status info to be stored by waitpid.
	char bglist[5][100] = {"", "", "", "", ""};
	char bgstatus[5][100] = {"", "", "", "", ""};
	pid_t bg_pids[5] = {-1, -1, -1, -1, -1}; // pids of each running bg process.
	int bg_num = 0; // Current bg process number.
	int bg_processes = 0; // Number of running bg processes.
	char *running = "[R]";
	char *stopped = "[S]";	

	for (;;) {

		char *directory = getcwd(NULL, 0);
		bool is_bg = false;

		// Error case for getcwd.
		if (directory == NULL) {
			fprintf(stderr, "\nCurrent working directory failure.\n");
			exit(EXIT_FAILURE);
		}
		
		// Receive input from user
		printf("%s", directory);
		char *cmd = readline (">");
		char **command = get_cmd(cmd);
		

		// Case where the command is empty.
		if (command[0] == NULL) {
			free(cmd);
			free(command);
			continue; // Skip to next command.
		}

		int n = 0;
		for (n = 0; n < 5; n++) {
			if (bg_pids[n] != -1) {
				if (waitpid(bg_pids[n], &status, WNOHANG) == bg_pids[n]) {
					char *str = "";
					strcpy(bglist[n], str);
					strcpy(bgstatus[n], str);
					bg_pids[n] = -1;
					bg_processes--;		
					printf("Background process %d has finished and will be removed from bglist.\n", n);
				}
			}
		}

		// Case where the user command is cd.
		if (strcmp("cd", command[0]) == 0) {
			int dir_change = chdir(command[1]);
			if (dir_change < 0) {
				fprintf(stderr, "\nDirectory change failure.\n");
				exit(EXIT_FAILURE);
			}
			continue; // Skip to next command.
		}


		// Case where user command is bglist.
		if (strcmp("bglist", command[0]) == 0) {
			int i = 0;
			printf("\n");
			for (i = 0; i < 5; i++) {
				printf("%s%s\n", bgstatus[i], bglist[i]);
			}
			printf("Total Background Processes: %d\n\n", bg_processes);
			continue; // Skip to next command.
		}	
		

		// Case where the user command is bgkill.
		if (strcmp("bgkill", command[0]) == 0) {
			
			if (command[1] == NULL) {
				fprintf(stderr, "No bg process was selected for the bgkill operation.\n");
				exit(EXIT_FAILURE);
			}
			
			// Input string converted to int.
			int process = atoi(command[1]);
			
			// Ensure that the selected process is valid and currently running.
			if ((process >= 0) && (process < 5) && (bg_pids[process] != -1)) {
				
				// If process is currently stopped, resume it before terminating it.
				if (strcmp(bgstatus[process], stopped) == 0) {
					kill(bg_pids[process], SIGCONT);
				}	

				kill(bg_pids[process], SIGTERM);
				printf("Background process %d has been terminated.\n", process);
			} else {
				fprintf(stderr, "There is no current bg process with the desired value.\n");
			}

			continue;
		}


		// Case where the user command is stop.
		if (strcmp("stop", command[0]) == 0) {
			
			if (command[1] == NULL) {
				fprintf(stderr, "No bg process was selected for the stop operation.\n");
				exit(EXIT_FAILURE);
			}
			
			// Input string converted to int.
			int process = atoi(command[1]);
			
			// Ensure that the selected process is valid and currently running.
			if ((process >= 0) && (process < 5) && (bg_pids[process] != -1)) {
				
				// If the process is running, stop it. Otherwise, display error message.
				if (strcmp(bgstatus[process], running) == 0) {
					kill(bg_pids[process], SIGSTOP);
					strcpy(bgstatus[process], stopped);
					printf("Background process %d has been stopped.\n", process);
				} else {
					fprintf(stderr, "The selected bg process is already stopped.\n");
				}

			} else {
				fprintf(stderr, "There is no current bg process with the desired value.\n");
			}

			continue;
		}


		// Case where the user command is start.
		if (strcmp("start", command[0]) == 0) {
			
			if (command[1] == NULL) {
				fprintf(stderr, "No bg process was selected for the start operation.\n");
				exit(EXIT_FAILURE);
			}
			
			// Input string converted to int.
			int process = atoi(command[1]);
			
			// Ensure that the selected process is valid and currently running.
			if ((process >= 0) && (process < 5) && (bg_pids[process] != -1)) {
				
				// If the process is stopped, start it. Otherwise, display error message.
				if (strcmp(bgstatus[process], stopped) == 0) {
					kill(bg_pids[process], SIGCONT);
					strcpy(bgstatus[process], running);
					printf("Background process %d has been resumed.\n", process);
				} else {
					fprintf(stderr, "The selected bg process is already running.\n");
				}

			} else {
				fprintf(stderr, "There is no current bg process with the desired value.\n");
			}

			continue;
		}


		// Case where user command is bg.
		if (strcmp("bg", command[0]) == 0) {
		
			is_bg = true;

			// Remove the bg command so the base command can be run.
			// Note: max number of arguments is 15 (17 total indices).
			int j = 0;
			for (j = 0; j < 16; j++) {
				command[j] = command[j+1];
		       	}
			
			if (bg_processes < 5) {
				int k = 0;
				for (k = 0; k < 5; k++) {
					
					// Stop at the first open entry in the bglist and add the new bg process there.
					if ((strcmp("", bglist[k]) == 0) && (strcmp("", bgstatus[k]) == 0)) {
						sprintf(bglist[k], "%d: %s %s", k, directory, command[0]);
						strcpy(bgstatus[k], running);
						printf("Background process has been started and added to bglist.\n");
						bg_num = k;
						bg_processes++;
						break;
					}
				}
			} else {
				printf("5 bg processes are already running. This is the maximum.\n");
			}
		}
		

		/* Fork and Exec operations begin. */

		child = fork();
		if (child < 0) {
			fprintf(stderr, "\nFork failure.\n");
			exit(EXIT_FAILURE);
		}	 
		
		// The child performs the specified command via execvp.
		if (child == 0) {

			int exec_call = execvp(command[0], command);

			if (exec_call < 0) {
				fprintf(stderr, "\nExec failure.\n");
				exit(EXIT_FAILURE);
			}
		
		// The parent checks all current bg processes to see if they have terminated, and removes them from bglist if required.
		} else {

			if (is_bg) bg_pids[bg_num] = child;

			int l = 0;
			for (l = 0; l < 5; l++) {
				if (bg_pids[l] != -1) {
					if (waitpid(bg_pids[l], &status, WNOHANG) == bg_pids[l]) {
						char *str = "";
						strcpy(bglist[l], str);
						strcpy(bgstatus[l], str);
						bg_pids[l] = -1;
						bg_processes--;
						printf("Background process %d has finished and will be removed from bglist.\n", l);
					}
				}
			}
			
			// If not a bg command, the parent also waits for the child process to finish executing.
			if (!is_bg) waitpid(child, &status, 0);
		}

		/* Fork and Exec operations end. */
		
		free(directory);
		free(cmd);
		free(command);
	}	

	return 0;
}

