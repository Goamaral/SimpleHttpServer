#include <stdlib.h>
#include <string.h>

#define	SERVER_STRING 	"Server: simpleserver/0.1.0\r\n"
#define HEADER_1	"HTTP/1.0 200 OK\r\n"
#define HEADER_2	"Content-Type: text/html\r\n\r\n"
#define GET_EXPR	"GET /"
#define CGI_EXPR	"cgi-bin/"
#define SIZE_BUF	1024

typedef struct Requests {
	int conn;
	char requiredFile[SIZE_BUF];
	time_t timeGetRequest;
	struct Requests *next;
} request_t;

void deleteRequestBuffer(request_t **request_buffer) {
  request_t *next;
  while((*request_buffer)->next!=NULL) {
    next = (*request_buffer)->next;
    free(*request_buffer);
    (*request_buffer)=next;
  }

  free(*request_buffer);
  *request_buffer=NULL;
}

request_t *createRequestBuffer(){
  request_t *head = malloc(sizeof(request_t));
  head->conn = -1;
  strcpy(head->requiredFile, "EMPTY");
  head->next = NULL;
  return head;
}

void remove_request(request_t **request_buffer, request_t **request_cpy, char scheduling[SIZE_BUF]) {
    request_t * next_node = NULL;

    if (*request_buffer == NULL || (*request_buffer)->conn == -1) {
				*request_cpy = NULL;
        return;
    }

		if( !strcmp(scheduling,"NORMAL") ) {
	    if( (next_node = (*request_buffer)->next) == NULL ) {
				next_node = createRequestBuffer();
			}

			if(!strcmp(scheduling,"FIFO")) {
		    *request_cpy = *request_buffer;
				(*request_cpy)->next=NULL;
		    *request_buffer = next_node;
			}
		}
}

void print_request_buffer(request_t *request_buffer) {
    request_t *current = request_buffer;

    while (current != NULL) {
      printf("conn: %d req: %s\n", current->conn, current->requiredFile);
      current = current->next;
    }
}

void add_request(request_t *request_buffer, int conn, char requiredFile[SIZE_BUF]) {
    request_t* current = request_buffer;

    if(current == NULL) {
      printf("Undefined buffer\n");
      exit(1);
    }

    if( (current->conn) == -1) {
      current->conn = conn;
      strcpy(current->requiredFile, requiredFile);
    } else {
      while (current->next != NULL) {
          current = current->next;
      }
      current->next = malloc(sizeof(request_t));
      current->next->conn = conn;
      strcpy(current->next->requiredFile,requiredFile);
      current->next->next = NULL;
    }
}
