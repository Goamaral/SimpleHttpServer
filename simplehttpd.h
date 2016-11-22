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

// Header of HTTP reply to client
#define	SERVER_STRING 	"Server: simpleserver/0.1.0\r\n"
#define HEADER_1	"HTTP/1.0 200 OK\r\n"
#define HEADER_2	"Content-Type: text/html\r\n\r\n"

#define GET_EXPR	"GET /"
#define CGI_EXPR	"cgi-bin/"
#define SIZE_BUF	1024
#define MAX_ALLOWED 10

void createThreadPool();
void createSharedMemory();
void *serve(void* id_ptr);
void statistics();
void send_page(int socket);
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
void createSemaphores();

// Produce debug information
#define DEBUG	  	1

//ERROR MACROS
#define THREADS 0
#define ALLOCATE_SHARED_MEMORY 1
#define ATTATCH_SHARED_MEMORY 2
#define SERVER 3
#define PID 4

typedef struct {
	int id;
  int conn;
  char requiredFile[SIZE_BUF];
  time_t timeGetRequest;
} serve_msg_t;
