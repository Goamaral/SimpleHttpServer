#include "simplehttpd.h"

typedef struct Config {
  int port;
  char allowed[MAX_ALLOWED][SIZE_BUF];
  char scheduling[SIZE_BUF];
  int threadpool;
} config_t;

//VARS
char buf[SIZE_BUF];

void printInvalidConfigFile(void);
void setAllowedFiles(char* str, char arr[MAX_ALLOWED][SIZE_BUF]);

// Read value of property from config file
void readParam(FILE *file) {
	int i=0;
	char c;
	int count = 0;

	do {
		 c = fgetc(file);
		 if( feof(file) || c=='\n' ) {
			 if(count==0) {
				 printInvalidConfigFile();
			 }
			 buf[i++]='\0';
			 count = 0;
			 break ;
		 }
		 if(count == 1) buf[i++]=c;
		 if(c=='=') count=1;
	} while(1);
}

// Print invalid config file and exit
void printInvalidConfigFile(void) {
	printf("Missing assignment = sign in port attribution in the config file\nConfig file must be of the following format:\n\n");
	printf("SERVERPORT=(port) -> Example:1234\n");
	printf("SCHEDULING=(schedule type) -> Example:NORMAL\n");
	printf("THREADPOOL=(number of threads) -> Example:5\n");
	printf("ALLLOWED=(allowed file names seperated by ; sign) -> file_a.html;file_b.html\n");
	printf("\nOnly .html and .hmtl.gz files supported\n");
	catch_ctrlc(SIGINT);
}

// Reads a line (of at most 'n' bytes) from socket
int read_line(int socket,int n) {
	int n_read;
	int not_eol;
	int ret;
	char new_char;

	n_read=0;
	not_eol=1;

	while (n_read<n && not_eol) {
		ret = read(socket,&new_char,sizeof(char));
		if (ret == -1) {
			printf("Error reading from socket (read_line)");
			return -1;
		}
		else if (ret == 0) {
			return 0;
		}
		else if (new_char=='\r') {
			not_eol = 0;
			// consumes next byte on buffer (LF)
			read(socket,&new_char,sizeof(char));
			continue;
		}
		else {
			buf[n_read]=new_char;
			n_read++;
		}
	}

	buf[n_read]='\0';
	#if DEBUG
	printf("read_line: new line read from client socket: %s\n",buf);
	#endif

	return n_read;
}

void getConfigData(config_t *config) {
  //READ CONFIG FILE

	FILE *file = fopen("config.txt","r");
	if(file == NULL) printInvalidConfigFile();

	 // Read port
	 readParam(file);
	 if( !isNumber(buf) ) {
		 printf("Invalid port value\n");
		 exit(1);
	 }
	 config->port=atoi(buf);
	 #if DEBUG
	 printf("port: %d\n", config->port);
	 #endif

	 //Read scheduling
	 readParam(file);
	 //TO DO -> create inStringArray function
	 /*if( !inStringArray(buf) ) {
		 printf("Invalid scheduling mode");
		 exit(1);
 	 }*/
	 strcpy(config->scheduling,buf);
	 #if DEBUG
	 printf("scheduling: %s\n", config->scheduling);
	 #endif

	 //Read threadpool
	 readParam(file);
	 if( !isNumber(buf) ) {
		 printf("Invalid number of threads\n");
		 exit(1);
	 }
	 config->threadpool = atoi(buf);
	 #if DEBUG
	 printf("threadpool: %d\n", config->threadpool);
	 #endif

   //Read allowed files
 	 readParam(file);
 	 setAllowedFiles(buf, config->allowed);
 	 #if DEBUG
 	 printf("Allowed files: %s; %s\n", config->allowed[0], config->allowed[1]);
 	 #endif

 	 fclose(file);
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
