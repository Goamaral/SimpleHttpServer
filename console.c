#include <stdio.h>
#include "simplehttpd.h"
#include <string.h>

#define N_HELPERS  3

//TODO -> add command syntax check

void printHelp() {
	char helpCommands[N_HELPERS][SIZE_BUF] = {" schedule <mode>\n\t-> mormal\n\t-> compressed\n\t-> static", " threadpool <number>", " allowed <filenames...>"};
	int i = 0;

	while(i<N_HELPERS) {
		printf("%s\n\n", helpCommands[i]);
		++i;
	}
}



int main() {
	char command[SIZE_BUF];
	int namedpipe;

	printf("--- Welcome to the configuration console ---\n");
	printf("To exit, type exit\n");
	printf("To see available commands, type help\n\n");

	// ignore ctrl + c
	signal(SIGINT, SIG_IGN);

	// read commands
	while(1) {
		printf(">> ");
		fgets(command, SIZE_BUF, stdin);
		// remove "\n"
		command[strlen(command)-1]=0;
		if(!strcmp(command, "exit")) break;
		else if(!strcmp(command, "help")) {
			printHelp();
        }
		else {

			// open pipe
			if( (namedpipe = open(PIPE_NAME, O_WRONLY)) == -1  ) {
				printf("Named pipe does not exist\n");
				printf("Please open local server\n");
				return 0;
			}

			// write on pipe
			if( write(namedpipe, command, SIZE_BUF * sizeof(char)) == -1 ) {
				printf("Error writting on pipe\n");
			}

			//close pipe
			if( close(namedpipe) == -1 ) {
				printf("Couldnt close pipe");
			}

			// open pipe
			if( open(PIPE_NAME, O_WRONLY) == -1 ) {
				printf("Named pipe does not exit\n");
				printf("Please open local server\n");
				return 0;
			}

			// cleanning pipe
			if( write(namedpipe, "", SIZE_BUF * sizeof(char)) == -1 ) {
				printf("Error writting on pipe");
			}
			
			// close pipe
			if( close(namedpipe) == -1 ) {
				printf("Couldnt close pipe\n");
			}
		}
	}

	return 0;
}
