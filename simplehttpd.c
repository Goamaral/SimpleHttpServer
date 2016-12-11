//Gonçalo Oliveira Amaral	2015249122 Quase todo o projecto
//Yushchynskyy Artem	2015251647	Apenas participou na primeira meta

#include "simplehttpd.h"

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

//Under maintenance variable
int underMaintenance = 0;

//consoleConnect VARIABLES
long consoleConnectId = 100;
pthread_t consoleConnectThreadId;

// SHARED MEMORY
int shmid;
shvar_t *sharedVar;

// Config
config_t *config;
config_t *new_config;

// THREADS, ID ARRAYS AND THREAD REQUESTS
long *id;
pthread_t *threads;
serve_msg_t *threadRequests;

// STATISTICS PROCESS PID and exit variable
pid_t statistics_PID;
int exitStats = 0;
int fd_stats;
char *addr_stats;
int len_stats;
int len_file_stats;

// SCHEDULER VARIABLES
int exitThreads;
int exitConsoleThread;
int *availableServingThreads;

int main(int argc, char **argv) {
	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);

	createNamedPipe();

	config = (config_t *)malloc(sizeof(config_t));

	if (getConfigData(config) == -1) {
		shutdown_server(CONFIG);
	}

	if (createSharedMemory() == -1) {
		shutdown_server(CLEAN_SHARED_MEMORY);
	}

	if (createSemaphores() == -1) {
		shutdown_server(CLEAN_SEMAPHORES);
	}

	exitThreads = 0;
	exitConsoleThread = 0;

	if(createConsoleConnectThread() == -1) {
		shutdown_server(CLEAN_SEMAPHORES);
	}

	if(createThreadPool() == -1) {
		shutdown_server(CLEAN_THREADS);
	}

	// CREATE STATISTICS PROCESS
	if ((statistics_PID = fork()) == 0) {
		statistics();
		exit(0);
	} else {
		if (statistics_PID == -1) {
			printf("Error creating statistics process\n");
			shutdown_server(STATS);
		}
		printf("Main PID: %d\n", getpid());
	}

	printf("Listening for HTTP requests on port %d\n", config->port);

	// Configure listening port
	if ((socket_conn = fireup(config->port)) == -1) {
		shutdown_server(STATS);
	}

	// CREATE REQUEST BUFFER AND SINGLE REQUEST NODE
	request = createRequestBuffer(request);
	request_buffer = createRequestBuffer(request_buffer);

	signal(SIGINT, catch_ctrlc);

	// Serve requests
	while (1) {
		// Accept connection on socket
		if ((new_conn = accept(socket_conn, (struct sockaddr *)&client_name, &client_name_len)) == -1) {
			printf("Error accepting connection\n");
			shutdown_server(STATS);
		}

		// Identify new client
		identify(new_conn);

		// Process request
		if (get_request(new_conn) == 1 && strcmp(req_buf, "favicon.ico") != 0) {
			#if DEBUG
			printf("org req: %s conn: %d\n", req_buf, new_conn);
			#endif

			//check if server is under maintenance
			if( underMaintenance ) {
				printf("Reloading new configurations or server under maintenance\n");
				send_page(new_conn, "server_maintenance.html");
				close(new_conn);
				continue;
			}

			// Unlock request buffer
			sem_wait(&requestBufferSemaphore);
			add_request(request_buffer, new_conn, req_buf);

			#if DEBUG
			printf("Added request\n");
			printf("\nREQUEST BUFFER:\n");
			print_request_buffer(request_buffer);
			#endif

			// Requests addded to the buffer
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
		shutdown_server(FAST_EXIT);
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

	#if DEBUG_THREAD
	printf("--- ");
	#endif

	for (i = 0; i < config->threadpool; ++i) {

		#if DEBUG_THREAD
		printf("%d ", availableServingThreads[i]);
		#endif

		if (availableServingThreads[i] == 0) {

			#if DEBUG_THREAD
			printf("---\n--- Thread %d available ---\n", i + 1);
			#endif

			return i;
		}
	}
	return -1;
}

int createConsoleConnectThread() {
	if (pthread_create(&consoleConnectThreadId, NULL, consoleConnect, (void *)&consoleConnectId)) {
			printf("Error creating consoleConnect\n");
			return -1;
	}
	#if DEBUG
	else {
			printf("--- Console Connect created ---\n");
	}
	#endif
	return 0;
}

int createThreadPool() {
    int i = 0;

    threadRequests = (serve_msg_t *)malloc((config->threadpool) * sizeof(serve_msg_t));
    id = (long *)malloc((config->threadpool + 1) * sizeof(long));
    threads = (pthread_t *)malloc((config->threadpool + 1) * sizeof(pthread_t));
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

    for (i = 0; i < config->threadpool; ++i) {
        availableServingThreads[i] = 0;
    }
    return 0;
}

void joinConsoleThread() {
	exitConsoleThread = 1;
	pthread_join(consoleConnectThreadId, NULL);

	#if DEBUG
	printf("Console Connect thread ended\n");
	#endif
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
    for (i = 0; i < config->threadpool + 1; ++i) {
        pthread_join(threads[i], NULL);
    }

    free(availableServingThreads);
    free(threadRequests);
    free(threads);
    free(id);
}

void *scheduler(void *id_ptr) {
	int targetedThread;
	char requiredFile[SIZE_BUF];
	FILE *fp;
	int allowed = 0;

	sem_wait(&startSchedulerSemaphore);

    #if DEBUG
    printf("--- Started scheduler ---\n");
    #endif

    while (1) {
        sem_wait(&requestAvailableSemaphore);
        sem_wait(&requestBufferSemaphore);
        if(exitThreads==1) break;
        remove_request(&request_buffer, &request, config->scheduling);
		sprintf(requiredFile, "htdocs/%s", request->requiredFile);
        sem_post(&requestBufferSemaphore);
        if (request != NULL && request->conn != -1) {
			allowed = allowedFile(request->requiredFile);
			if ((fp = fopen(requiredFile, "rt")) == NULL || !allowed) {
				printf("send_page: page %s not found, alerting client\n", requiredFile);
				send_page(request->conn, "404.html");
				close(request->conn);
				deleteRequestBuffer(&request);
				continue;
			}
            if ((targetedThread = checkFreeThread()) != -1) {
                threadRequests[targetedThread].conn = request->conn;
                strcpy(threadRequests[targetedThread].requiredFile, request->requiredFile);
				threadRequests[targetedThread].timeGetRequest = request->timeGetRequest;

                deleteRequestBuffer(&request);

				#if DEBUG_THREAD
				printf("\n--- SENDING MESSAGE TO SERVING THREAD %d ---\n", targetedThread + 1);
				#endif

                availableServingThreads[targetedThread] = 1;
                sem_post(&threadSemaphores[targetedThread]);
            } else {
                printf("No available threads at the moment\n");
                send_page(request->conn, "server_overload.html");
                close(request->conn);
            }
        }
				//Always serve request then exit
				if(exitThreads == 1) break;
    }

    #if DEBUG
    long threadId = *((long *)id_ptr);
    printf("Scheduler as thread %ld has ended\n", threadId);
    #endif

    pthread_exit(NULL);
}

int allowedFile(char *file) {
	int n_allowed = (int)sizeof(config->allowed) / SIZE_BUF;
	int i;

	for(i=0;i<n_allowed;++i){
		if( !strcmp(config->allowed[i], file) ) {
			return 1;
		}
	}

	return 0;
}

void *consoleConnect(void *id_ptr) {
	int err;
	char command[SIZE_BUF];
	char operation[SIZE_BUF];
	char extra[SIZE_BUF];
	char *token;
	char separator[2]= " ";
	int i;

	#if DEBUG
	printf("--- Console Connect started ---\n");
	#endif

	if( (namedpipe = open(PIPE_NAME, O_RDONLY|O_NONBLOCK)) == -1 ) {
		printf("Error creating named pipe\n");
		pthread_exit(0);
	}

	#if DEBUG
	printf("Named pipe created\n");
	#endif

	while (1) {
		if (exitConsoleThread == 1) break;

		// read console command
		while( (err = read(namedpipe, command, SIZE_BUF * sizeof(char))) != 0);
		// checks if console command exists or was saved successfully
		if(err == -1) {
			#if DEBUG
			printf("Error reading command\n");
			#endif
		} else if(!strcmp(command, "")) {
			#if DEBUG
			printf("Command not inserted\n");
			#endif
		} else {
			#if DEBUG
			printf("Command read: %s\n", command);
			#endif

			//Stop listening to ctrl + c
			signal(SIGINT, SIG_IGN);
			underMaintenance = 1;

			sscanf(command, "%s %[^\n\t]", operation, extra);

			if(!strcmp(operation,"schedule")) {
				strupr(extra);
				if(!strcmp(extra,config->scheduling)) continue;
				strcpy(config->scheduling, extra);
			}

			if(!strcmp(operation,"threadpool")) {
				if(config->threadpool == atoi(extra)) continue;
				joinThreads();
				destroySemaphores();
				#if DEBUG
				printf("Will change threadpool value");
				#endif
				config->threadpool = atoi(extra);
				createSemaphores();
				createThreadPool();
			}

			if(!strcmp(operation,"allowed")) {
				token = strtok(extra, separator);
				i=0;
				while(token!=NULL && i<MAX_ALLOWED) {
					strcpy(config->allowed[i], token);
					++i;
					token = strtok(NULL, separator);
				}
			}

			printf("Changes applied\n");

			underMaintenance = 0;

			signal(SIGINT, catch_ctrlc);

			exitThreads=0;
		}
	}

    close(namedpipe);

    unlink(PIPE_NAME);

    #if DEBUG
    long threadId =*((long*)id_ptr); printf("--- Console Connect at thread %ld has ended ---\n", threadId);
    #endif

    pthread_exit(NULL);
}

void strupr(char s[]) {
   int c = 0;

   while (s[c] != '\0') {
      if (s[c] >= 'a' && s[c] <= 'z') {
         s[c] = s[c] - 32;
      }
      c++;
   }
}

void *serve(void *id_ptr) {
	long threadId = *((long *)id_ptr);

	while (1) {
		sem_wait(&threadSemaphores[threadId - 1]);
		if (exitThreads == 1) break;

		#if DEBUG_THREAD
		printf("--- RECIEVED WORK AT THREAD %ld ---\n", threadId);
		printf("MSG\nreq:%s\nconn:%d\n\n", threadRequests[threadId - 1].requiredFile, threadRequests[threadId - 1].conn);
		#endif

		send_page(threadRequests[threadId - 1].conn, threadRequests[threadId - 1].requiredFile);
		close(threadRequests[threadId - 1].conn);

		#if SLEEP
		sleep(10);
		#endif

		gettimeofday(&(threadRequests[threadId - 1].timeProcessed), 0);

		sem_wait(&(sharedVar->semaphore));
		strcpy(sharedVar->requiredFile, threadRequests[threadId - 1].requiredFile);
		sharedVar->timeProcessed = threadRequests[threadId - 1].timeProcessed;
		sharedVar->timeGetRequest = threadRequests[threadId - 1].timeGetRequest;
		sharedVar->treated = 0;
		sem_post(&(sharedVar->semaphore));

		#if DEBUG_TRHEAD
		printf("--- THREAD %ld FREE AGAIN ---\n", threadId);
		#endif

		availableServingThreads[threadId - 1] = 0;

		//Always serve request then exit
		if(exitThreads == 1) break;
	}

	#if DEBUG
	printf("Served thread %ld\n", threadId);
	#endif

	pthread_exit(NULL);
}

void print_stats(int sig) {
	int i;
	int counter = 0;
	char buf[SIZE_BUF];
	int n_compressed = 0;
	int n_static = 0;
	char type[SIZE_BUF];
	int seconds1, seconds2;
	float mseconds1, mseconds2;
	float compressed_mseconds_sum = 0, static_mseconds_sum = 0;
	float diff;
	const char separator[2] = "|";
	char *token;
	char date1[SIZE_BUF], date2[SIZE_BUF];

	for(i=strlen("--- SERVER LOGS ---\n");i<len_stats;++i) {
		if(addr_stats[i]=='\n' || addr_stats[i]=='\0') {
			buf[counter] = '\0';
			counter = 0;

			token = strtok(buf, separator);//type
			strcpy(type, token);

			token = strtok(NULL, separator);//filename
			token = strtok(NULL, separator);//date1
			strcpy(date1, token);
			token = strtok(NULL, separator);
			strcpy(date2, token);

			sscanf(date1, "%*d:%*d:%d %f", &seconds1, &mseconds1);
			sscanf(date2, "%*d:%*d:%d %f", &seconds2, &mseconds2);

			diff = (seconds2*1000 + mseconds2) - (seconds1*1000 + mseconds1);
			if(!strcmp("compressed", type)) {
				compressed_mseconds_sum += diff;
				n_compressed += 1;
			} else {
				static_mseconds_sum += diff;
				n_static += 1;
			}
		} else {
			buf[counter]=addr_stats[i];
			counter+=1;
		}
	}

	printf("Numero de pedidos estaticos: %d\n", n_static);
	printf("Numero de pedidos comprimidos: %d\n", n_compressed);
	printf("Tempo medio para servir pedidos estaticos: %f milisegundos\n", static_mseconds_sum / (float)n_static);
	printf("Tempo medio para servir pedidos comprimidos: %f milisegundos\n", compressed_mseconds_sum / (float)n_compressed);
}

void reset_stats(int sig) {
	struct stat st;
	FILE *err_file;

	if( munmap(addr_stats,len_stats) == -1 ) {
		perror("nunmap error");
		return;
	}
	close(fd_stats);

	err_file = fopen("server.log", "w+");
	fputs("--- SERVER LOGS ---\n", err_file);
	fclose(err_file);

	if( (fd_stats = open("server.log", O_RDWR | O_APPEND)) == -1) {
		perror("open");
		return;
	}

	if (fstat(fd_stats, &st) == -1) {
		perror("fstat error");
		return;
	}

	len_file_stats = st.st_size;
	len_stats = len_file_stats;

	addr_stats = mmap(NULL, len_file_stats, PROT_WRITE|PROT_READ, MAP_SHARED, fd_stats, 0);

	if(addr_stats == MAP_FAILED) {
		perror("nmap error: ");
		return;
	}

	return;
}

void statistics() {
	struct stat st;
	FILE *err_file;
	char export[SIZE_BUF];
	const char separator[2] = ".";
	char *token;
	char ext[SIZE_BUF];
	char date1[SIZE_BUF];
	time_t time1;
	struct tm *timeinfo1;
	char date2[SIZE_BUF];
	time_t time2;
	struct tm *timeinfo2;

	signal(SIGINT, catch_ctrlc_stats);
	signal(SIGUSR1, print_stats);
	signal(SIGUSR2, reset_stats);

	printf("Statistics PID: %d\n", getpid());

	sem_wait(&(sharedVar->semaphore));
	sharedVar->treated = 1;
	sem_post(&(sharedVar->semaphore));

	if( (fd_stats = open("server.log", O_RDWR | O_APPEND)) == -1) {
		if(errno == 2) {
			err_file = fopen("server.log", "w+");
			fputs("--- SERVER LOGS ---\n", err_file);
			fclose(err_file);
			if( (fd_stats = open("server.log", O_RDWR | O_APPEND)) == -1) {
				printf("FILE ERROR %s\n", strerror(errno));
				return;
			}
		} else {
			printf("FILE ERROR %s\n", strerror(errno));
			return;
		}
	}

	if (fstat(fd_stats, &st) == -1) {
		perror("fstat error");
		close(fd_stats);
		return;
	}

	len_file_stats = st.st_size;
	len_stats = len_file_stats;

	addr_stats = mmap(NULL, len_file_stats, PROT_WRITE|PROT_READ, MAP_SHARED, fd_stats, 0);

	if(addr_stats == MAP_FAILED) {
		perror("nmap error: ");
		close(fd_stats);
		return;
	}

	while(1) {
		sem_wait(&(sharedVar->semaphore));
		if(exitStats == 1) break;
		if(sharedVar->treated == 0){
			token = strtok(sharedVar->requiredFile, separator);

			while( token != NULL ) {
				strcpy(ext, token);
				token = strtok(NULL, separator);
			}

			if(!strcmp("gz", ext)) {
				strcpy(sharedVar->type, "compressed" );
			} else {
				strcpy(sharedVar->type, "static" );
			}

			time1 = sharedVar->timeGetRequest.tv_sec;
			time2 = sharedVar->timeProcessed.tv_sec;

			timeinfo1 = localtime(&time1);
			timeinfo2 = localtime(&time2);

			strftime(date1, SIZE_BUF, "%H:%M:%S", timeinfo1);
			strftime(date2, SIZE_BUF, "%H:%M:%S", timeinfo2);

			sprintf(export, "%s|%s|%s %.2f|%s %.2f\n",
				sharedVar->type,
				sharedVar->requiredFile,
				date1,
				(double)sharedVar->timeGetRequest.tv_usec / 1000,
				date2,
				(double)sharedVar->timeProcessed.tv_usec / 1000);

			#if DEBUG_STATISTICS
			printf("%s\n", export);
			#endif

			len_stats = len_file_stats;
			len_file_stats += strlen(export);
			if (ftruncate(fd_stats, len_file_stats) != 0)
			{
				perror("ftruncate error");
				shutdown_stats();
				return;
			}

			addr_stats = mremap(addr_stats, len_stats, len_file_stats, MREMAP_MAYMOVE);

			if(addr_stats == MAP_FAILED) {
				perror("mremap error: ");
				shutdown_stats();
				return;
			}

			memcpy(addr_stats+len_stats, export, len_file_stats - len_stats);

			sharedVar->treated = 1;
		}
		sem_post(&(sharedVar->semaphore));
	}

	shutdown_stats();
	return;
}

void catch_ctrlc_stats(int sig) {
	sem_post(&(sharedVar->semaphore));
	exitStats = 1;
	shutdown_stats();
}

void shutdown_stats() {
	if( munmap(addr_stats,len_stats) == -1 ) perror("nunmap error");
	close(fd_stats);
}

int createSemaphores() {
	int i;

	threadSemaphores = (sem_t *)malloc((config->threadpool) * sizeof(sem_t));

	if (sem_init(&requestBufferSemaphore, 0, 1) == -1)
		return -1;
	if (sem_init(&startSchedulerSemaphore, 0, 0) == -1)
		return -1;
	if (sem_init(&requestAvailableSemaphore, 0, 0) == -1)
		return -1;
	if(sem_init(&(sharedVar->semaphore), 0 , 1) == -1)
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
  shmid = shmget(IPC_PRIVATE, sizeof(shvar_t), IPC_CREAT | 0766);
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
	int status;
	printf("--- Server terminating ---\n");

	switch(op) {
		case CLEAN_SERVER:
			close(new_conn);
			close(socket_conn);
			deleteRequestBuffer(&request_buffer);
			joinConsoleThread();
		case STATS:
			waitpid(statistics_PID, &status, WNOHANG|WUNTRACED);
			printf("Statistics is out\n");
		case CLEAN_THREADS:
			joinThreads();
		case CLEAN_SEMAPHORES:
			destroySemaphores();
		case CLEAN_SHARED_MEMORY:
			destroySharedMemory();
		case CONFIG:
			free(config);
		case FAST_EXIT:
			break;
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
			sprintf(command, "gunzip -f %s", zippedFullPath);
			system(command);
			compressed = 1;
		}
	}

	send_header(socket);

	#if DEBUG
	printf("send_page: sending page %s to client\n", buf_tmp);
	#endif

	fp = fopen(buf_tmp, "rt");

	while (fgets(buf_tmp, SIZE_BUF, fp)) {
		send(socket, buf_tmp, strlen(buf_tmp), 0);
	}

	if (compressed == 1) {
		// If a compressed file has been required, delete it after sent
		sprintf(command, "gzip %s%s", path, unzippedName); // htdocs/index.html
		system(command);
	}

	// Close file
	fclose(fp);
	return;
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
	#if DEBUG
	int port;
	#endif
	struct sockaddr_storage addr;

	len = sizeof addr;
	getpeername(socket, (struct sockaddr *)&addr, &len);

	// Assuming only IPv4
	s = (struct sockaddr_in *)&addr;
	#if DEBUG
	port = ntohs(s->sin_port);
	#endif
	inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

	#if DEBUG
	printf("identify: received new request from %s port %d\n", ipstr, port);
	#endif

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
