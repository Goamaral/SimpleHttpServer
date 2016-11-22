#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "config.h"
#include "request.h"

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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/msg.h>

#define DEBUG	  	1
#define SIZE_BUF	1024
#define FAST_EXIT 0
#define CLEAN_SHARED_MEMORY 1
#define CLEAN_SEMAPHORES 2
#define CLEAN_THREADS 3
#define CLEAN_SERVER 4

typedef struct {
	int id;
  int conn;
  char requiredFile[SIZE_BUF];
  time_t timeGetRequest;
} serve_msg_t;

int createThreadPool();
void *serve(void* id_ptr);
void *scheduler(void* id_ptr);
int createSharedMemory();
void destroySharedMemory();
void statistics();
void send_page(int socket, char req_buf[SIZE_BUF]);
int get_request(int socket);
void send_header(int socket);
void execute_script(int socket);
void identify(int socket);
int fireup(int port);
void not_found(int socket);
void cannot_execute(int socket);
void joinThreads();
void detatchSharedMemory();
void desallocateSharedMemory();
void catch_ctrlc(int sig);
void shutdown_server(int option);
int read_line(int socket,int n);
int createSemaphores();
