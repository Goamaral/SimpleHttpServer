#include "simplehttpd.h"
// TODO -> insert baby protection
// TODO -> assign ctrl + c signal to statistics process
// TODO -> create othre type of schedules
// TODO -> separate create/join scheduler and console from other threads

// REQUEST BUFFER
request_t *request;
request_t *request_buffer;

// NAMEDPIPE FILE DESCRIPTOR
int namedpipe;

// BUFFERS DECLARATION
char buf[SIZE_BUF];
char req_buf[SIZE_BUF];
int socket_conn, new_conn;

// SEMAPHORES IDS
sem_t requestBufferSemaphore;
sem_t startSchedulerSemaphore;
sem_t *threadSemaphores;
sem_t requestAvailableSemaphore;

// SHARED MEMORY
int shmid;
int *sharedVar;

// Config
config_t *config;
config_t *new_config;

// THREADS, ID ARRAYS AND THREAD REQUESTS
long *id;
pthread_t *threads;
serve_msg_t *threadRequests;

// STATISTICS PROCESS PID
pid_t statistics_PID;

// SCHEDULER VARIABLES
int exitThreads;
int *availableServingThreads;

int main(int argc, char **argv) {
	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);

	// CREATE STATISTICS PROCESS
	if ((statistics_PID = fork()) == 0) {
		statistics();
		exit(0);
	} else {
		if (statistics_PID == -1) {
			printf("Error creating statistics process\n");
			shutdown_server(FAST_EXIT);
		}
		printf("Main PID: %d\n", getpid());
	}

	createNamedPipe();

	config = (config_t *)malloc(sizeof(config_t));

	if (getConfigData(config) == -1) {
		shutdown_server(FAST_EXIT_2);
	}

	if (createSharedMemory() == -1) {
		shutdown_server(CLEAN_SHARED_MEMORY);
	}

	if (createSemaphores() == -1) {
		shutdown_server(CLEAN_SEMAPHORES);
	}

	exitThreads = 0;

	if (createThreadPool() == -1) {
		shutdown_server(CLEAN_THREADS);
	}

	printf("Listening for HTTP requests on port %d\n", config->port);

	// Configure listening port
	if ((socket_conn = fireup(config->port)) == -1) {
		shutdown_server(CLEAN_THREADS);
	}

	// CREATE REQUEST BUFFER AND SINGLE REQUEST NODE
	request = createRequestBuffer(request);
	request_buffer = createRequestBuffer(request_buffer);

	signal(SIGINT, catch_ctrlc);

	// Serve requests
	while (1) {
		// Accept connection on socket
		if ((new_conn = accept(socket_conn, (struct sockaddr *)&client_name,
                           &client_name_len)) == -1) {
		printf("Error accepting connection\n");
		shutdown_server(CLEAN_THREADS);
	}

	// Identify new client
	identify(new_conn);

	// Process request
	if (get_request(new_conn) == 1 && strcmp(req_buf, "favicon.ico") != 0) {
		#if DEBUG
		printf("org req: %s conn: %d\n", req_buf, new_conn);
		#endif

		// Unlock request buffer
		sem_wait(&requestBufferSemaphore);
		add_request(request_buffer, new_conn, req_buf);

		#if DEBUG
		printf("Added request\n");
		printf("\nREQUEST BUFFER:\n");
		print_request_buffer(request_buffer);
		#endif

		// Increases number of available threads
		sem_post(&requestAvailableSemaphore);

		// Unlocks request buffer
		sem_post(&requestBufferSemaphore);

		// Starts scheduler after first added request
		int firstRequestAdded;
		sem_getvalue(&startSchedulerSemaphore, &firstRequestAdded);
		if (firstRequestAdded == 0)
			sem_post(&startSchedulerSemaphore);
		} else if (strcmp(req_buf, "favicon.ico") == 0) {
			#if DEBUG
			printf("--- Ignored favicon.ico request ---\n");
			#endif
			close(new_conn);
		}
	}
	return 0;
}

int createNamedPipe() {
	// Creates the named pipe if it doesn't exist yet
	if ((mkfifo(PIPE_NAME, O_CREAT | O_EXCL | 0766) < 0) && (errno != EEXIST)) {
		printf("Couldnt create the namedpipe\n");
		shutdown_server(FAST_EXIT_2);
	}

	#if DEBUG
	else {
		printf("Named pipe created\n");
	}
	#endif

	return 0;
}

int checkFreeThread() {
  int i;
  printf("--- ");
  for (i = 0; i < config->threadpool; ++i) {
    printf("%d ", availableServingThreads[i]);
    if (availableServingThreads[i] == 0) {
      printf("---\n--- Thread %d available ---\n", i + 1);
      return i;
    }
  }
  return -1;
}

int createThreadPool() {
    int i = 0;

    threadRequests = (serve_msg_t *)malloc((config->threadpool) * sizeof(serve_msg_t));
    id = (long *)malloc((config->threadpool + 2) * sizeof(long));
    threads = (pthread_t *)malloc((config->threadpool + 2) * sizeof(pthread_t));
    availableServingThreads = (int *)malloc((config->threadpool) * sizeof(int));
    
    id[i]=i;

    if (pthread_create(&threads[i], NULL, scheduler, (void *)&id[i])) {
        printf("Error creating scheduler\n");
        return -1;
    }
    #if DEBUG
    else {
        printf("--- Scheduler created ---\n");
    }
    #endif
    
    for (i = 1; i < config->threadpool + 1; i++) {
        id[i] = i;
        if (pthread_create(&threads[i], NULL, serve, (void *)&id[i])) {
            printf("Error creating threads\n");
            return -1;
        }

        #if DEBUG
        printf("Thread %d created\n", (int)id[i]);
        #endif
    }
    
    id[i]=i;

    if (pthread_create(&threads[i], NULL, consoleConnect, (void *)&id[i])) {
        printf("Error creating consoleConnect\n");
        return -1;
    }
    #if DEBUG 
    else {
        printf("--- Console Connect created ---\n");
    }
    #endif

    for (i = 0; i < config->threadpool; ++i) {
        availableServingThreads[i] = 0;
    }
    return 0;
}

void joinThreads() {
    int i;
    exitThreads = 1;

    sem_post(&startSchedulerSemaphore);
    sem_post(&requestAvailableSemaphore);

    for (i = 0; i < config->threadpool + 1; ++i) {
        sem_post(&threadSemaphores[i]);
    }

    // Wait for all threads to complete
    for (i = 0; i < config->threadpool + 2; ++i) {
        pthread_join(threads[i], NULL);
    }

    free(availableServingThreads);
    free(threadRequests);
    free(threads);
    free(id);
}

void *scheduler(void *id_ptr) {
    int targetedThread;
    sem_wait(&startSchedulerSemaphore);

    #if DEBUG
    printf("--- Started scheduler ---\n");
    #endif

    while (exitThreads != 1) {
        sem_wait(&requestAvailableSemaphore);
        sem_wait(&requestBufferSemaphore);
        if(exitThreads==1) break;
        remove_request(&request_buffer, &request, config->scheduling);
        sem_post(&requestBufferSemaphore);

        if (request != NULL && request->conn != -1) {
            if ((targetedThread = checkFreeThread()) != -1) {
                threadRequests[targetedThread].conn = request->conn;
                strcpy(threadRequests[targetedThread].requiredFile, request->requiredFile);

                deleteRequestBuffer(&request);

                printf("\n--- SENDING MESSAGE TO SERVING THREAD %d ---\n", targetedThread + 1);
                availableServingThreads[targetedThread] = 1;
                sem_post(&threadSemaphores[targetedThread]);
            } else {
                printf("No available threads at the moment\n");
                send_page(request->conn, "server_overload.html");
                close(request->conn);
            }
        }
    }

    #if DEBUG
    long threadId = *((long *)id_ptr);
    printf("Scheduler as thread %ld has ended\n", threadId);
    #endif

    pthread_exit(NULL);
}

void *consoleConnect(void *id_ptr) {
	int err;
	char command[SIZE_BUF];
	char operation[SIZE_BUF];

	#if DEBUG
	printf("--- Console Connect started ---\n");
	#endif

	if( (namedpipe = open(PIPE_NAME, O_RDONLY|O_NONBLOCK)) == -1 ) {
		printf("Error creating named pipe\n");
		pthread_exit(0);
	}

	while (1) {
		if (exitThreads == 1) break;

		new_config = (config_t *)malloc(sizeof(config_t));

		// read console command
		while( (err = read(namedpipe, command, SIZE_BUF * sizeof(char))) != 0);
		// checks if console command exists or was saved successfully
		if(err == -1) {
			#if DEBUG
			printf("Error reading command\n");
			#endif
		} else if(!strcmp(command, "")) {
			#if DEBUG
			//printf("Command not inserted\n");
			#endif
		} else {
			#if DEBUG
			printf("Command read: %s\n", command);
			#endif

			sscanf(command, "%s %s", operation, extra);

			new_config = (config_t*)malloc(sizeof(config_t));

			new_config->port = config->port;

			//TODO -> check command and change new config

			free(new_config);
		}
	}

    close(namedpipe);

    unlink(PIPE_NAME);

    #if DEBUG
    long threadId =*((long*)id_ptr); printf("--- Console Connect at thread %ld has ended ---\n", threadId);
    #endif

    pthread_exit(NULL);
}

void *serve(void *id_ptr) {
    long threadId = *((long *)id_ptr);

    #if DEBUG
    printf("--- WAITING FOR MESSAGE OF TYPE %ld ---\n", threadId);
    #endif

    while (1) {
        sem_wait(&threadSemaphores[threadId - 1]);
        if (exitThreads == 1) break;

        #if DEBUG
        printf("--- RECIEVED MESSAGE AT THREAD %ld ---\n", threadId);
        printf("MSG\nreq:%s\nconn:%d\n\n", threadRequests[threadId - 1].requiredFile, threadRequests[threadId - 1].conn);
        #endif

        send_page(threadRequests[threadId - 1].conn,
        threadRequests[threadId - 1].requiredFile);
        close(threadRequests[threadId - 1].conn);

        #if SLEEP
        sleep(10);
        #endif

        printf("--- THREAD %ld FREE AGAIN ---\n", threadId);
        availableServingThreads[threadId - 1] = 0;
    }
    
    close(threadRequests[threadId - 1].conn);

    #if DEBUG
    printf("Served thread %ld\n", threadId);
    #endif

    pthread_exit(NULL);
}

// TO DO -> create statistics function
void statistics() { printf("Statistics PID: %d\n", getpid()); }

int createSemaphores() {
  int i;

  threadSemaphores = (sem_t *)malloc((config->threadpool) * sizeof(sem_t));

  if (sem_init(&requestBufferSemaphore, 0, 1) == -1)
    return -1;
  if (sem_init(&startSchedulerSemaphore, 0, 0) == -1)
    return -1;
  if (sem_init(&requestAvailableSemaphore, 0, 0) == -1)
    return -1;
  for (i = 0; i < config->threadpool; ++i) {
    if (sem_init(&threadSemaphores[i], 0, 0) == -1)
      return -1;
  }
  return 0;
}

void destroySemaphores() {
  int i;

  sem_destroy(&requestBufferSemaphore);
  sem_destroy(&startSchedulerSemaphore);
  sem_destroy(&requestAvailableSemaphore);
  for (i = 0; i < config->threadpool; ++i) {
    sem_destroy(&threadSemaphores[i]);
  }

  free(threadSemaphores);
}

int createSharedMemory() {
  shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0766);
  if (shmid == -1) {
    printf("--- Error allocating shared memory segment ---\n");
    return -1;
  }
  sharedVar = shmat(shmid, NULL, 0);
  if (config == (void *)-1) {
    printf("--- Error attatching shared memory ---\n");
    return -1;
  }
  return 0;
}

void destroySharedMemory() {
  if (shmdt(sharedVar) == -1) {
    printf("--- Error detaching memory ---\n");
    return;
  }

  if (shmctl(shmid, IPC_RMID, NULL) == -1) {
    printf("--- Error desalocating shared memory segment ---\n");
  }
}

void catch_ctrlc(int sig) {
  printf("--- Caught ctrl c ---\n");
  shutdown_server(CLEAN_SERVER);
}

// Closes socket before closing
void shutdown_server(int op) {
  printf("--- Server terminating ---\n");

  switch (op) {
  case CLEAN_SERVER:
    close(new_conn);
    close(socket_conn);
    deleteRequestBuffer(&request_buffer);
  case CLEAN_THREADS:
    joinThreads();
  /*case CLEAN_SEMAPHORES:
    destroySemaphores();
  case CLEAN_SHARED_MEMORY:
    destroySharedMemory();
  case FAST_EXIT_2:
    free(config);
    kill(statistics_PID, SIGKILL);
  case FAST_EXIT:
    break;*/
  }

  exit(op + 1);
}

int read_line(int socket, int n) {
  int n_read;
  int not_eol;
  int ret;
  char new_char;

  n_read = 0;
  not_eol = 1;

  while (n_read < n && not_eol) {
    ret = read(socket, &new_char, sizeof(char));
    if (ret == -1) {
      printf("Error reading from socket (read_line)");
      return -1;
    } else if (ret == 0) {
      return 0;
    } else if (new_char == '\r') {
      not_eol = 0;
      // consumes next byte on buffer (LF)
      read(socket, &new_char, sizeof(char));
      continue;
    } else {
      buf[n_read] = new_char;
      n_read++;
    }
  }

  buf[n_read] = '\0';
#if DEBUG
  printf("read_line: new line read from client socket: %s\n", buf);
#endif

  return n_read;
}

// Send html page to client
void send_page(int socket, char req_buf[SIZE_BUF]) {
  FILE *fp;
  char buf_tmp[SIZE_BUF];
  char *ext;
  char path[SIZE_BUF];
  char unzippedName[SIZE_BUF];
  char fileName[SIZE_BUF];
  char zippedFullPath[SIZE_BUF];
  int compressed = 0;
  char command[SIZE_BUF];

  // Searchs for page in directory htdocs
  sprintf(buf_tmp, "htdocs/%s", req_buf);

#if DEBUG
  printf("send_page: searching for %s\n", buf_tmp);
#endif

  ext = strrchr(req_buf, '.');

#if DEBUG
  printf("--- File extention %s ---\n", ext + 1);
#endif

  if (strcmp(ext, ".")) {
    if (!strcmp(ext, ".gz")) {
      strcpy(path, "htdocs/");
      strncpy(unzippedName, req_buf, strlen(req_buf) - 3);       // index.html
      strncpy(fileName, unzippedName, strlen(unzippedName) - 5); // index
      sprintf(zippedFullPath, "%s%s", path, req_buf); // htdocs/index.html.gz
      sprintf(buf_tmp, "%s%s", path, unzippedName);   // htdocs/index.html
      printf("--- test: %s ---\n", buf_tmp);
      sprintf(command, "gunzip -k -f %s", zippedFullPath);
      system(command);
      compressed = 1;
    }
  }

  // Verifies if file doesnt exist
  if ((fp = fopen(buf_tmp, "rt")) == NULL) {
    // Page not found, send error to client
    printf("send_page: page %s not found, alerting client\n", buf_tmp);
    send_page(socket, "404.html");
  } else {
    send_header(socket);

#if DEBUG
    printf("send_page: sending page %s to client\n", buf_tmp);
#endif

    while (fgets(buf_tmp, SIZE_BUF, fp))
      send(socket, buf_tmp, strlen(buf_tmp), 0);

    if (compressed == 1) {
      // If a compressed file has been required, delete it after sent
      sprintf(command, "%s%s", path, unzippedName); // htdocs/index.html
      system(command);
    }

    // Close file
    fclose(fp);
    return;
  }
}

// Processes request from client
int get_request(int socket) {
  int i, j;
  int found_get;

  found_get = 0;
  while (read_line(socket, SIZE_BUF) > 0) {
    if (!strncmp(buf, GET_EXPR, strlen(GET_EXPR))) {
      // GET received, extract the requested page/script
      found_get = 1;
      i = strlen(GET_EXPR);
      j = 0;
      while ((buf[i] != ' ') && (buf[i] != '\0'))
        req_buf[j++] = buf[i++];
      req_buf[j] = '\0';
    }
  }

  // Currently only supports GET
  if (!found_get) {
    printf("--- Request from client without a GET ---\n");
    return -1;
  }
  // If no particular page is requested then we consider htdocs/index.html
  if (!strlen(req_buf))
    sprintf(req_buf, "index.html");

#if DEBUG
  printf("get_request: client requested the following page: %s\n", req_buf);
#endif

  return 1;
}

// Send message header (before html page) to client
void send_header(int socket) {
#if DEBUG
  printf("send_header: sending HTTP header to client\n");
#endif
  sprintf(buf, HEADER_1);
  send(socket, buf, strlen(HEADER_1), 0);
  sprintf(buf, SERVER_STRING);
  send(socket, buf, strlen(SERVER_STRING), 0);
  sprintf(buf, HEADER_2);
  send(socket, buf, strlen(HEADER_2), 0);

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
  getpeername(socket, (struct sockaddr *)&addr, &len);

  // Assuming only IPv4
  s = (struct sockaddr_in *)&addr;
  port = ntohs(s->sin_port);
  inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

  printf("identify: received new request from %s port %d\n", ipstr, port);

  return;
}

// Creates, prepares and returns new socket
int fireup(int port) {
  int new_sock;
  struct sockaddr_in name;

  // Creates socket
  if ((new_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
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

  return (new_sock);
}

// Send a 5000 internal server error (script not configured for execution)
void cannot_execute(int socket) {
  sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
  send(socket, buf, strlen(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(socket, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(socket, buf, strlen(buf), 0);
  sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
  send(socket, buf, strlen(buf), 0);

  return;
}
