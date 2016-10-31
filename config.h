// Produce debug information
//#define DEBUG	  	1

typedef struct Config {
  int port;
  char allowed[MAX_ALLOWED][SIZE_BUF];
  char scheduling[SIZE_BUF];
  int threadpool;
} config_t;

//VARS
char buf[SIZE_BUF];

int isNumber(char buff[SIZE_BUF]);
void readParam(FILE *file);
void printInvalidConfigFile(void);
void getConfigData(config_t *config);
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
	printf("\nMissing assignment = sign in one of the attributions in the config file\nConfig file must be of the following format:\n\n");
	printf("SERVERPORT=(port) -> Example:1234\n");
	printf("SCHEDULING=(schedule type) -> Example:NORMAL\n");
	printf("THREADPOOL=(number of threads) -> Example:5\n");
	printf("ALLLOWED=(allowed file names seperated by ; sign) -> file_a.html;file_b.html\n");
	printf("\nOnly .html and .hmtl.gz files supported\n\n");
  shutdown_server("PID");
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

void getConfigData(config_t *config) {
  //READ CONFIG FILE

	FILE *file = fopen("config.txt","r");
	if(file == NULL) printInvalidConfigFile();

	 // Read port
	 readParam(file);
	 if( !isNumber(buf) ) {
		 printf("Invalid port value\n");
     shutdown_server("PID");
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
	 if( !isNumber(buf) || atoi(buf)==0) {
		 printf("Invalid number of threads\n");
     shutdown_server("PID");
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
