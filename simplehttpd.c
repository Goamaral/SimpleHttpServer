//Run as: gcc simplehttpd.c semlib.c -lpthread -D_REENTRANT -Wall -o run

#include "simplehttpd.h"
#include "request.h"
#include "config.h"
#include "semlib.h"

//REQUEST BUFFER
request_t *request;
request_t *request_buffer;

//BUFFERS DECLARATION
char buf[SIZE_BUF];
char req_buf[SIZE_BUF];
char buf_tmp[SIZE_BUF];
int socket_conn,new_conn;

//SEMAPHORES IDS
int semid;

//SHARED MEMORY
int shmid;
config_t *config;

//THREADS AND ID ARRAYS
long *id;
pthread_t *threads;

//STATISTICS PROCESS PID
pid_t statistics_PID;

int main(int argc, char ** argv) {
	int i;
	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);

	//CREAT REQUEST BUFFER AND SINGLE REQUEST NODE
	request = createRequestBuffer(request);
	request_buffer = createRequestBuffer(request_buffer);

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

	//SHARED MEMORY INITIALIZATION
	shmid = shmget(IPC_PRIVATE,sizeof(config_t),IPC_CREAT|0766);
	if(shmid==-1) {
		printf("Error allocating shared memory segment\n");
		exit(1);
	}
	config = shmat(shmid, NULL, 0);
	if(config == (void *) -1) {
		printf("Error attatching shared memory\n");
		catch_ctrlc(SIGINT);
	}
	semid = sem_get(1,1);
	if(semid==-1) {
		printf("Error creating semaphore\n");
		catch_ctrlc(SIGINT);
	}

	getConfigData(config);

	if(config->threadpool <= 0) {
		printf("Invalid number of threadpool\n");
		catch_ctrlc(SIGINT);
	}
	id = (long*) malloc(config->threadpool * sizeof(long));
	threads = (pthread_t*) malloc(config->threadpool * sizeof(pthread_t));

	//INTIALIZE THREADPOOL
	for (i = 0; i < config->threadpool; i++) {
		id[i] = i;
		if(pthread_create(&threads[i], NULL, serve, (void *)&id[i])) {
			printf("Error creating threads\n");
			catch_ctrlc(SIGINT);
		}
		#if DEBUG
		printf("Thread %d created\n", (int)id[i]);
		#endif
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
			exit(1);
		}

		// Identify new client
		identify(new_conn);

		// Process request
		get_request(new_conn);
		//TO DO -> save new_conn and requested file name

		printf("org req: %s conn: %d\n", req_buf, new_conn);

		add_request(request_buffer, new_conn, req_buf);
		printf("\nREQUEST BUFFER:\n");
		print_request_buffer(request_buffer);
		request_buffer = remove_request(&request_buffer,&request);
		printf("\nREQUEST:\n");
		print_request_buffer(request);

		//TO DO -> control flow from here on
		// Verify if request is for a page or script
		if(!strncmp(request->requiredFile,CGI_EXPR,strlen(CGI_EXPR)))
			execute_script(request->conn);
		else
			// Search file with html page and send to client
			send_page(request->conn);
		// Terminate connection with client
		close(request->conn);
	}

	return 0;
}

//TO DO -> create serve function
void *serve(void* id_ptr) {
	#if DEBUG
	long id = *((long *) id_ptr);
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
	#if DEBUG
	printf("Cleaning up\n");
	#endif

	//KILL STATISTICS PROCESS
	kill(statistics_PID, SIGKILL);

	//CLOSE SEMAPHORE
	sem_close(semid);

	//Wait for all threads to complete
	for (i = 0; i < config->threadpool; i++) {
		pthread_join(threads[i], NULL);
	}


	//DEALLOCATE AND DETACH SHARED MEMORY
	if(shmdt(config)==-1) {
		printf("Error detaching memory\n");
	}
	if(shmctl(shmid,IPC_RMID, NULL)==-1) {
		printf("Error desalocating shared memory segment\n");
	};

	//FREE VARS
	free(threads);
	free(id);
	deleteRequestBuffer(&request);
	//deleteRequestBuffer(&request_buffer);
	exit(0);
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
