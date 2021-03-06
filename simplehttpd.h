//Gonçalo Oliveira Amaral	2015249122 Quase todo o projecto
//Yushchynskyy Artem	2015251647	Apenas participou na primeira meta

#define _GNU_SOURCE
#include "config.h"
#include "request.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/mman.h>

//#define DEBUG 1 // when on prints debugging messages
//#define SLEEP 1 // when on makes threads take longer to serve
//#define DEBUG_THREAD 1 // when on prints debugging messages for threads
#define DEBUG_STATISTICS 1 // when on prints stat debugging messages

#define SIZE_BUF 1024

#define CONFIG 0
#define CLEAN_SHARED_MEMORY 1
#define CLEAN_SEMAPHORES 2
#define CLEAN_THREADS 3
#define STATS 4
#define CLEAN_SERVER 5
#define FAST_EXIT 6

#define PIPE_NAME "Server To Config Console Pipe"

typedef struct {
	int id;
	int conn;
	char requiredFile[SIZE_BUF];
	struct timeval timeGetRequest;
	struct timeval timeProcessed;
} serve_msg_t;

typedef struct {
	char type[SIZE_BUF];
	char requiredFile[SIZE_BUF];
	struct timeval timeGetRequest;
	struct timeval timeProcessed;
	int treated;
	sem_t semaphore;
} shvar_t;


void catch_ctrlc_stats(int sig);
void shutdown_stats();
int allowedFile(char *file);
int createNamedPipe();
int createThreadPool();
void *serve(void *id_ptr);
void *consoleConnect(void *id_ptr);
void *scheduler(void *id_ptr);
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
int read_line(int socket, int n);
int createSemaphores();
void destroySemaphores();
int createConsoleConnectThread();
void strupr(char s[]);
