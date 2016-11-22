#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SIZE_BUF	1024
#define MAX_ALLOWED 10

typedef struct Config {
  int port;
  char allowed[MAX_ALLOWED][SIZE_BUF];
  char scheduling[SIZE_BUF];
  int threadpool;
} config_t;

//VARS
char buf[SIZE_BUF];

int isNumber(char buff[SIZE_BUF]);
int readParam(FILE *file);
int printInvalidConfigFile(void);
int getConfigData(config_t *config);
void setAllowedFiles(char* str, char arr[MAX_ALLOWED][SIZE_BUF]);


// Read value of property from config file
int readParam(FILE *file) {
	int i=0;
	char c;
	int count = 0;

	do {
		 c = fgetc(file);
		 if( feof(file) || c=='\n' ) {
			 if(count==0) {
				 return -1;
			 }
			 buf[i++]='\0';
			 count = 0;
			 break ;
		 }
		 if(count == 1) buf[i++]=c;
		 if(c=='=') count=1;
	} while(1);
	return 0;
}

// Print invalid config file and exit
int printInvalidConfigFile(void) {
	printf("\nMissing assignment = sign in one of the attributions in the config file\nConfig file must be of the following format:\n\n");
	printf("SERVERPORT=(port) -> Example:1234\n");
	printf("SCHEDULING=(schedule type) -> Example:NORMAL\n");
	printf("THREADPOOL=(number of threads) -> Example:5\n");
	printf("ALLLOWED=(allowed file names seperated by ; sign) -> file_a.html;file_b.html\n");
	printf("\nOnly .html and .hmtl.gz files supported\n\n");
  return -1;
}

int isNumber(char* string){
    int i = 0;

		if(!strcmp(string,"")) return 0;

    while (string[i] != '\0'){
        if (string[i] < '0' || string[i] > '9')
            return 0;
        i++;
    }

    return 1;
}

int getConfigData(config_t *config) {
	FILE *file = fopen("config.txt","r");
	if(file == NULL) return printInvalidConfigFile();

	 // Read port
	 if( readParam(file) == -1 ) return printInvalidConfigFile();
	 if( !isNumber(buf) ) {
		 printf("--- Invalid port value ---\n");
     return -1;
	 }
	 config->port=atoi(buf);
	 #if DEBUG
	 printf("port: %d\n", config->port);
	 #endif

	 //Read scheduling
	 if( readParam(file) == -1 ) return printInvalidConfigFile();

   /*
	 //TO DO -> create inStringArray function
	 if( !inStringArray(buf) ) {
		 printf("Invalid scheduling mode");
		 exit(1);
 	 }
   */

	 strcpy(config->scheduling,buf);
	 #if DEBUG
	 printf("scheduling: %s\n", config->scheduling);
	 #endif

	 //Read threadpool
	 if( readParam(file) == -1 ) return printInvalidConfigFile();
	 if( !isNumber(buf) || atoi(buf)==0) {
		 printf("Invalid number of threads\n");
     return -1;
	 }
	 config->threadpool = atoi(buf);
	 #if DEBUG
	 printf("threadpool: %d\n", config->threadpool);
	 #endif

   //Read allowed files
	 if( readParam(file) == -1 ) return printInvalidConfigFile();
 	 setAllowedFiles(buf, config->allowed);
 	 #if DEBUG
 	 printf("Allowed files: %s; %s\n", config->allowed[0], config->allowed[1]);
 	 #endif

 	 fclose(file);
	 return 0;
}

// Set array of strings from string
void setAllowedFiles(char* str, char arr[MAX_ALLOWED][SIZE_BUF]) {
  char buf_tmp[SIZE_BUF];
	int i=0,j=0,a=0;
	while(str[i]!='\0') {
		if(str[i]==';') {
			buf_tmp[j]='\0';
			strcpy(arr[a++],buf_tmp);
			j=0;
		} else {
			buf_tmp[j++]= str[i];
		}
		++i;
	}
	buf_tmp[j]='\0';
	strcpy(arr[a],buf_tmp);
}
