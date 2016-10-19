#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#define SIZE_BUF	1024

typedef struct Requests {
  int ready;
	int conn;
	char requiredFile[SIZE_BUF];
	time_t timeGetRequest;
	time_t timeServedRequest;
	struct Requests *next;
	struct Requests *prev;
} request_t;

//FUNCTIONS
void print_request_buffer();
void add_request(int conn);
void remove_request();

//STRUCTS
request_t *request_buffer = NULL;

int main() {
  add_request(10);
  add_request(20);
  print_request_buffer();
  remove_request();
  printf("-----\n");
  print_request_buffer();
  return 0;
}

//TO DO -> create function
//      -> depends on scheduling(default: FIFO)
//      -> destroys link
//      -> sets next and prev to NULL
//      -> copy to shared memory

void remove_request() {
  request_t *curr;
  request_t *next;
  if(request_buffer!=NULL) {
    curr = request_buffer;
    next = curr-> next;
  }else return;

  free(curr);
  next->prev = NULL;
  request_buffer = next;
}

//TO DO -> complete function
void print_request_buffer() {
  request_t *curr = NULL;

  if(request_buffer==NULL) {
    printf("Empty List\n");
  } else curr = request_buffer;

  while(curr!=NULL){
    printf("conn: %d\n", curr->conn);
    curr = curr->next;
  }
}

//TO DO -> complete function
void add_request(int conn) {
  request_t *curr = NULL;
  request_t *request = (request_t*) malloc(sizeof(request_t));
  request->conn = conn;

  if(request_buffer==NULL) {
    request_buffer = request;
    return;
  } else curr = request_buffer;

  while(curr->next!=NULL) {
    curr = curr->next;
  }

  curr->next = request;
}
