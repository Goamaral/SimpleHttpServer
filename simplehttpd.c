#include "simplehttpd.h"
#include "request.h"
#include "config.h"
#include <semaphore.h>

//REQUEST BUFFER
request_t *request;
request_t *request_buffer;

//BUFFERS DECLARATION
char buf[SIZE_BUF];
char req_buf[SIZE_BUF];
char buf_tmp[SIZE_BUF];
int socket_conn,new_conn;

//SEMAPHORES IDS
sem_t requestBufferSemaphore;
sem_t startSchedulerSemaphore;
sem_t availableRequests;
//TO DO -> create semaphore for each thread

//SHARED MEMORY
int shmid;
config_t *config;

//THREADS AND ID ARRAYS
long *id;
pthread_t *threads;

//STATISTICS PROCESS PID
pid_t statistics_PID;

//SCHEDULER VARIABLES
int schedulerOn;
int *availableServingThreads;
serve_msg_t *threadMessages;
int messageQueueId;

//REQUEST COUNTER
int requestCounter=0;

//THREAD CALLED ID
long msg_sent;

int main(int argc, char ** argv) {
	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);

	//CREATE REQUEST BUFFER AND SINGLE REQUEST NODE
	request = createRequestBuffer(request);
	request_buffer = createRequestBuffer(request_buffer);

	//CREATE STATISTICS PROCESS
	if( (statistics_PID=fork()) == 0) {
			statistics();
			exit(0);
	} else {
		if(statistics_PID==-1) {
			printf("Error creating statistics process\n");
			shutdown_server(PID);
		}
		printf("Main PID: %d\n", getpid());
	}

	createSharedMemory();

	//CREATE SEMAPHORES -> Error catcher
	sem_init(&requestBufferSemaphore, 0, 1);
	sem_init(&startSchedulerSemaphore, 0, 0);
	sem_init(&availableRequests, 0, 0);
	//

	getConfigData(config);

	//AVAILABLE SERVING THREADS
	availableServingThreads = (int*) malloc( (config->threadpool) * sizeof(int) );
	threadMessages = (serve_msg_t*) malloc( (config->threadpool) * sizeof(serve_msg_t) );

	//VARIABLES FOR THE THREADPOOL
	id = (long*) malloc( (config->threadpool + 1) * sizeof(long));
	threads = (pthread_t*) malloc( (config->threadpool + 1) * sizeof(pthread_t));

	createThreadPool();

	//MESSAGE QUEUE
	if( (messageQueueId=msgget(IPC_PRIVATE, IPC_CREAT|0766)) == -1 ) {
    printf("Error creating message queue");
		shutdown_server(THREADS);
  }

	signal(SIGINT,catch_ctrlc);

	//FIRE UP THE A SERVER CONNECTION
	printf("Listening for HTTP requests on port %d\n", config->port);

	// Configure listening port
	if ((socket_conn=fireup(config->port))==-1)
		exit(1);

	// Serve requests
	while (1)
	{
		// Accept connection on socket
		if ( (new_conn = accept(socket_conn,(struct sockaddr *)&client_name,&client_name_len)) == -1 ) {
			printf("Error accepting connection\n");
			shutdown_server(SERVER);
		}

		// Identify new client
		identify(new_conn);

		// Process request
		get_request(new_conn);
		requestCounter+=1;

		//Unlock request buffer
    sem_wait(&requestBufferSemaphore);
		printf("Added request\n");
		printf("org req: %s conn: %d\n", req_buf, new_conn);
		add_request(request_buffer, requestCounter, new_conn, req_buf);
		printf("\nREQUEST BUFFER:\n");
		print_request_buffer(request_buffer);
		//Increases number of available threads
		sem_post(&availableRequests);
		//Unlocks request buffer
    sem_post(&requestBufferSemaphore);
		//Starts scheduler after first added request
		int firstRequestAdded;
		sem_getvalue(&startSchedulerSemaphore, &firstRequestAdded);
		if(firstRequestAdded == 0) sem_post(&startSchedulerSemaphore);
  }

	return 0;
}

int checkFreeThread() {
	int i;
	for(i=0; i < config->threadpool; ++i) {
		if(availableServingThreads[i] == 0) {
			return i;
		}
	}
	return -1;
}

//TO DO-> create scheduler function
//TEST: send message  to turn on scheduler and serving threads instead of variable
void *scheduler(void* id_ptr) {
	int mtype;
	sem_wait(&startSchedulerSemaphore);
	printf("Started scheduler\n");
	
	while(schedulerOn) {
		//TO DO-> nao remover da lista mas ficar com o id do request e depois de servido apaga-lo da lista
		//FOR FIFO ONLY

		sem_wait(&availableRequests);
		sem_wait(&requestBufferSemaphore);
		remove_request(&request_buffer,&request);
		sem_post(&requestBufferSemaphore);

		if(request!=NULL && request->conn!=-1) {
			//TO DO-> create fill message function
			if( (mtype = checkFreeThread()) != -1 ) {
				threadMessages[mtype].conn = request->conn;
				strcpy(threadMessages[mtype].requiredFile, request->requiredFile);

				printf("\nREQUEST:\n");
				print_request_buffer(request);
				deleteRequestBuffer(&request);

				printf("\n--- SENDING MESSAGE TO SERVING THREAD %d ---\n", mtype+1);
				printf("MSG\nreq:%s\nconn:%d\n\n", threadMessages[mtype].requiredFile, threadMessages[mtype].conn);
				availableServingThreads[mtype]=1;
				msg_sent=mtype+1;
			} else {
				printf("No available threads at the moment\n");
				//TO DO-> Send full server to client
			}
		}
	}
	#if DEBUG
	long threadId = *((long *) id_ptr);
	printf("Scheduler as thread %ld has ended\n", threadId);
	#endif
	pthread_exit(NULL);
}

//INTIALIZE THREADPOOL
void createThreadPool() {
	int i=0;
	schedulerOn=1;

	if(pthread_create(&threads[i], NULL, scheduler, (void *)&id[i])) {
		printf("Error creating threads\n");
		shutdown_server(THREADS);
	}
	for (i = 1; i < config->threadpool + 1; i++) {
		id[i] = i;
		if(pthread_create(&threads[i], NULL, serve, (void *)&id[i])) {
			printf("Error creating threads\n");
			shutdown_server(THREADS);
		}
		#if DEBUG
		printf("Thread %d created\n", (int)id[i]);
		#endif
	}

	for(i=0; i < config->threadpool; ++i) {
		availableServingThreads[i]=0;
	}
}

//SHARED MEMORY INITIALIZATION
void createSharedMemory() {
	shmid = shmget(IPC_PRIVATE,sizeof(config_t),IPC_CREAT|0766);
	if(shmid==-1) {
		printf("Error allocating shared memory segment\n");
		shutdown_server(ALLOCATE_SHARED_MEMORY);
	}
	config = shmat(shmid, NULL, 0);
	if(config == (void *) -1) {
		printf("Error attatching shared memory\n");
		shutdown_server(ATTATCH_SHARED_MEMORY);
	}
}

//TO DO -> create serve function | change to no wait
void *serve(void* id_ptr) {
	long id = *((long *) id_ptr);

	printf("\n--- WAITING FOR MESSAGE OF TYPE %ld ---\n", id);
	while(schedulerOn) {
		if(msg_sent==id) {
			msg_sent=0;
			printf("\n--- RECIEVED MESSAGE AT THREAD %ld ---\n", id);
			printf("MSG\nreq:%s\nconn:%d\n\n", threadMessages[id-1].requiredFile, threadMessages[id-1].conn);
			strcpy(req_buf,threadMessages[id-1].requiredFile);
			send_page(threadMessages[id-1].conn);
			close(threadMessages[id-1].conn);
			availableServingThreads[id-1]=1;
		}
	}
	printf("Served thread %ld\n", id);
	pthread_exit(NULL);
}

//TO DO -> create statistics function
void statistics() {
	printf("Statistics PID: %d\n", getpid());
}

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
		shutdown_server(SERVER);
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

//JOIN THREADS
void joinThreads() {
	schedulerOn=0;
	int i;
	//Wait for all threads to complete
	for (i = 0; i < config->threadpool+1; i++) {
		pthread_join(threads[i], NULL);
	}

	free(availableServingThreads);
	free(threadMessages);
	free(threads);
	free(id);
}

//DETATCH MEMORY
void detatchSharedMemory() {
	if(shmdt(config)==-1) {
		printf("Error detaching memory\n");
		return;
	}
}

//DESALLOCATE MEMORY
void desallocateSharedMemory() {
	if(shmctl(shmid,IPC_RMID, NULL)==-1) {
		printf("Error desalocating shared memory segment\n");
	}
}

void catch_ctrlc(int sig) {
	shutdown_server(SERVER);
}

// Closes socket before closing
void shutdown_server(int option) {
	#if DEBUG
	printf("Cleaning up\n");
	#endif

	//KILL STATISTICS PROCESS
	kill(statistics_PID, SIGKILL);

	switch(option) {
    case THREADS:
		  joinThreads();
			desallocateSharedMemory();
  		detatchSharedMemory();
      break;
  	case ALLOCATE_SHARED_MEMORY:
  		desallocateSharedMemory();
      break;
  	case ATTATCH_SHARED_MEMORY:
  		desallocateSharedMemory();
  		detatchSharedMemory();
      break;
  	case SERVER:
  		joinThreads();
  		desallocateSharedMemory();
  		detatchSharedMemory();
			sem_destroy(&requestBufferSemaphore);
			sem_destroy(&startSchedulerSemaphore);

  		printf("Server terminating\n");
  		close(socket_conn);

  		//FREE VARS
  		deleteRequestBuffer(&request_buffer);
      break;
    default:
      break;
	}

	exit(0);
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
