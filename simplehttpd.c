/*
 Run as: gcc simplehttpd.c semlib.c -lpthread -D_REENTRANT -Wall -o run

 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "semlib.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

// Produce debug information
#define DEBUG	  	1

// Header of HTTP reply to client
#define	SERVER_STRING 	"Server: simpleserver/0.1.0\r\n"
#define HEADER_1	"HTTP/1.0 200 OK\r\n"
#define HEADER_2	"Content-Type: text/html\r\n\r\n"

#define GET_EXPR	"GET /"
#define CGI_EXPR	"cgi-bin/"
#define SIZE_BUF	1024
#define MAX_ALLOWED 10

int  fireup(int port);
void identify(int socket);
void get_request(int socket);
int  read_line(int socket, int n);
void send_header(int socket);
void send_page(int socket);
void execute_script(int socket);
void not_found(int socket);
void catch_ctrlc(int);
void cannot_execute(int socket);

void statistics(void);

int isNumber(char* string);
void *serve(void* id_ptr);
void setAllowedFiles(char* str, char arr[MAX_ALLOWED][SIZE_BUF]);
void printInvalidConfigFile(void);
void readParam(FILE *file);

//Struct defenition and decaration for requests
typedef struct Requests {
	int conn;
	char requiredFile[SIZE_BUF];
	time_t timeGetRequest;
	time_t timeServedRequest;
	struct Requests *next;
	struct Requests *prev;
} request_t;

struct Requests request_buffer;

//BUFFERS DECLARATION
char buf[SIZE_BUF];
char req_buf[SIZE_BUF];
char buf_tmp[SIZE_BUF];
int socket_conn,new_conn;
// CONFIG FILE DATA
int port;
char allowed[MAX_ALLOWED][SIZE_BUF];
char scheduling[SIZE_BUF];
int threadpool;

//SEMAPHORES IDS
int shmid, semid;

//SHARED MEMORY
struct Requests *shared_struct;

//THREADS AND ID ARRAYS
long *id;
pthread_t *threads;

//STATISTICS PROCESS PID
pid_t statistics_PID;

int main(int argc, char ** argv) {
	int i;
	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);

	//CREATE STATISTICS PROCESS
	if( (statistics_PID=fork()) == 0) {
			statistics();
			exit(0);
	} else {
		if(statistics_PID==-1) {
			printf("Error creating statistics process\n");
			exit(1);
		}
		printf("Main PID: %d\n", getpid());
	}

	//READ CONFIG FILE
	FILE *config = fopen("config.txt","r");
	if(config == NULL) printInvalidConfigFile();

	 // Read port
	 readParam(config);
	 if( !isNumber(buf) ) {
		 printf("Invalid port value\n");
		 exit(1);
	 }
	 port=atoi(buf);
	 #if DEBUG
	 printf("port: %i\n",port);
	 #endif

	 //Read scheduling
	 readParam(config);
	 //TO DO -> create validScheduling function
	 /*if( !validScheduling(buf) ) {
		 printf("Invalid scheduling mode");
		 exit(1);
 	 }*/
	 strcpy(scheduling,buf);
	 #if DEBUG
	 printf("scheduling: %s\n", scheduling);
	 #endif

	 //Read threadpool
	 readParam(config);
	 if( !isNumber(buf) ) {
		 printf("Invalid number of threads\n");
		 exit(1);
	 }
	 threadpool = atoi(buf);
	 #if DEBUG
	 printf("threadpool: %d\n", threadpool);
	 #endif
	 //TO DO -> check if theardpool is bigger than 0
	 id = (long*) malloc(threadpool * sizeof(long));
	 threads = (pthread_t*) malloc(threadpool * sizeof(pthread_t));

	 //Read allowed files
	 readParam(config);
	 strcpy(buf_tmp,buf);
	 setAllowedFiles(buf_tmp, allowed);
	 #if DEBUG
	 printf("Allowed files: %s; %s\n", allowed[0], allowed[1]);
	 #endif

	fclose(config);

	//SHARED MEMORY INITIALIZATION
	shmid = shmget(IPC_PRIVATE,sizeof(int),IPC_CREAT|0766);
	if(shmid==-1) {
		printf("Error allocating shared memory segment\n");
		exit(1);
	}
	shared_struct = shmat(shmid, NULL, 0);
	if(shared_struct == (void *) -1) {
		printf("Error attatching shared memory\n");
		catch_ctrlc(SIGINT);
	}
	semid = sem_get(1,1);
	if(semid==-1) {
		printf("Error creating semaphore\n");
		catch_ctrlc(SIGINT);
	}

	(*shared_struct).next = NULL;
	(*shared_struct).prev = NULL;

	//INTIALIZE THREADPOOL
	for (i = 0; i < threadpool; i++) {
		id[i] = i;
		if(pthread_create(&threads[i], NULL, serve, (void *)&id[i])) {
			printf("Error creating threads\n");
			catch_ctrlc(SIGINT);
		}
		printf("Thread %d created\n", i);
  }

	signal(SIGINT,catch_ctrlc);

	//FIRE UP THE A SERVER CONNECTION
	printf("Listening for HTTP requests on port %d\n",port);

	// Configure listening port
	if ((socket_conn=fireup(port))==-1)
		exit(1);

	// Serve requests
	while (1)
	{
		// Accept connection on socket
		if ( (new_conn = accept(socket_conn,(struct sockaddr *)&client_name,&client_name_len)) == -1 ) {
			printf("Error accepting connection\n");
			exit(1);
		}

		// Identify new client
		identify(new_conn);

		// Process request
		get_request(new_conn);
		//TO DO -> save new_conn and requested file name

		//TO DO -> control flow from here on
		// Verify if request is for a page or script
		if(!strncmp(req_buf,CGI_EXPR,strlen(CGI_EXPR)))
			execute_script(new_conn);
		else
			// Search file with html page and send to client
			send_page(new_conn);

		// Terminate connection with client
		close(new_conn);

	}

}

int isNumber(char* string){
    int i = 0;

    while (string[i] != '\0'){
        if (string[i] < '0' || string[i] > '9')
            return 0;
        i++;
    }
    return 1;
}

//TO DO -> create serve function
void *serve(void* id_ptr) {
	long id = *((long *) id_ptr);
	#if DEBUG
	printf("Served thread %ld\n", id);
	#endif
	pthread_exit(NULL);
}

//TO DO -> create statistics function
void statistics() {
	printf("Statistics PID: %d\n", getpid());
}

//TO DO -> allow only allowed pages
//TO DO -> show 404 page if page not found
// Send html page to client
void send_page(int socket) {
	FILE * fp;

	// Searchs for page in directory htdocs
	sprintf(buf_tmp,"htdocs/%s",req_buf);

	#if DEBUG
	printf("send_page: searching for %s\n",buf_tmp);
	#endif

	// Verifies if file exists
	if((fp=fopen(buf_tmp,"rt"))==NULL) {
		// Page not found, send error to client
		printf("send_page: page %s not found, alerting client\n",buf_tmp);
		not_found(socket);
	}
	else {
		// Page found, send to client

		// First send HTTP header back to client
		send_header(socket);

		printf("send_page: sending page %s to client\n",buf_tmp);
		while(fgets(buf_tmp,SIZE_BUF,fp))
			send(socket,buf_tmp,strlen(buf_tmp),0);

		// Close file
		fclose(fp);
	}

	return;

}

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

// Set array of strings from string
void setAllowedFiles(char* str, char arr[MAX_ALLOWED][SIZE_BUF]) {
	//TRY OUT -> use str[++i]
	int i=0,j=0,a=0;
	while(str[i]!='\0') {
		if(str[i]==';') {
			buf[j]='\0';
			strcpy(arr[a++],buf);
			j=0;
		} else {
			buf[j++]= str[i];
		}
		++i;
	}
	buf[j]='\0';
	strcpy(arr[a],buf);
}

// Processes request from client
void get_request(int socket) {
	int i,j;
	int found_get;

	found_get=0;
	while ( read_line(socket,SIZE_BUF) > 0 ) {
		if(!strncmp(buf,GET_EXPR,strlen(GET_EXPR))) {
			// GET received, extract the requested page/script
			found_get=1;
			i=strlen(GET_EXPR);
			j=0;
			while( (buf[i]!=' ') && (buf[i]!='\0') )
				req_buf[j++]=buf[i++];
			req_buf[j]='\0';
		}
	}

	// Currently only supports GET
	if(!found_get) {
		printf("Request from client without a GET\n");
		exit(1);
	}
	// If no particular page is requested then we consider htdocs/index.html
	if(!strlen(req_buf))
		sprintf(req_buf,"index.html");

	#if DEBUG
	printf("get_request: client requested the following page: %s\n",req_buf);
	#endif

	return;
}

// Send message header (before html page) to client
void send_header(int socket) {
	#if DEBUG
	printf("send_header: sending HTTP header to client\n");
	#endif
	sprintf(buf,HEADER_1);
	send(socket,buf,strlen(HEADER_1),0);
	sprintf(buf,SERVER_STRING);
	send(socket,buf,strlen(SERVER_STRING),0);
	sprintf(buf,HEADER_2);
	send(socket,buf,strlen(HEADER_2),0);

	return;
}

// Execute script in /cgi-bin
void execute_script(int socket) {
	// Currently unsupported, return error code to client
	cannot_execute(socket);

	return;
}

// Identifies client (address and port) from socket
void identify(int socket) {
	char ipstr[INET6_ADDRSTRLEN];
	socklen_t len;
	struct sockaddr_in *s;
	int port;
	struct sockaddr_storage addr;

	len = sizeof addr;
	getpeername(socket, (struct sockaddr*)&addr, &len);

	// Assuming only IPv4
	s = (struct sockaddr_in *)&addr;
	port = ntohs(s->sin_port);
	inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

	printf("identify: received new request from %s port %d\n",ipstr,port);

	return;
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

// Creates, prepares and returns new socket
int fireup(int port) {
	int new_sock;
	struct sockaddr_in name;

	// Creates socket
	if ((new_sock = socket(PF_INET, SOCK_STREAM, 0))==-1) {
		printf("Error creating socket\n");
		return -1;
	}

	// Binds new socket to listening port
 	name.sin_family = AF_INET;
 	name.sin_port = htons(port);
 	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(new_sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		printf("Error binding to socket\n");
		return -1;
	}

	// Starts listening on socket
 	if (listen(new_sock, 5) < 0) {
		printf("Error listening to socket\n");
		return -1;
	}

	return(new_sock);
}

// Sends a 404 not found status message to client (page not found)
void not_found(int socket) {
 	sprintf(buf,"HTTP/1.0 404 NOT FOUND\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,SERVER_STRING);
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"Content-Type: text/html\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<HTML><TITLE>Not Found</TITLE>\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<BODY><P>Resource unavailable or nonexistent.\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"</BODY></HTML>\r\n");
	send(socket,buf, strlen(buf), 0);

	return;
}

// Send a 5000 internal server error (script not configured for execution)
void cannot_execute(int socket) {
	sprintf(buf,"HTTP/1.0 500 Internal Server Error\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"Content-type: text/html\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<P>Error prohibited CGI execution.\r\n");
	send(socket,buf, strlen(buf), 0);

	return;
}

// Closes socket before closing
void catch_ctrlc(int sig) {
	int i;
	printf("Server terminating\n");
	close(socket_conn);

	//KILL STATISTICS PROCESS
	#if DEBUG
	printf("Killing Statistics process\n");
	#endif
	kill(statistics_PID, SIGKILL);

	//CLOSE SEMAPHORE AND DETACH SHARED STRUCT
	#if DEBUG
	printf("Closing semaphore\n");
	#endif
	sem_close(semid);
	#if DEBUG
	printf("Detaching shared memory\n");
	#endif
	shmdt(shared_struct);
	shmctl(shmid,IPC_RMID, NULL);

	//Wait for all threads to complete
	#if DEBUG
	printf("Waiting for all threads to complete\n");
	#endif
	for (i = 0; i < threadpool; i++) {
		pthread_join(threads[i], NULL);
	}

	//Free threads and id arrays
	free(threads);
	free(id);
	exit(0);
}
