#define SIZE_BUF	1024

typedef struct Requests {
	int conn;
	char requiredFile[SIZE_BUF];
	time_t timeGetRequest;
	time_t timeServedRequest;
	struct Requests *next;
	struct Requests *prev;
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

void remove_request(request_t **request_buffer, request_t **request_cpy) {
    request_t * next_node = NULL;

    if (*request_buffer == NULL) {
				*request_cpy = NULL;
        return;
    }

    if( (next_node = (*request_buffer)->next) == NULL ) {
			next_node = createRequestBuffer();
		}
    *request_cpy = *request_buffer;
		(*request_cpy)->next=NULL;
    *request_buffer = next_node;
}

//TO DO -> complete function
void print_request_buffer(request_t *request_buffer) {
    request_t *current = request_buffer;

    while (current != NULL) {
      printf("conn: %d req: %s\n", current->conn, current->requiredFile);
      current = current->next;
    }
}

//TO DO -> complete function
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
