#ifndef _HELPER_H_
#define _HELPER_H_

#include <stdio.h>

#define dbprintf(...) do{if(DEBUG) fprintf(stderr, __VA_ARGS__); }while(0)

#define MAX_SOCK 1024
#define BUF_SIZE 4096

#define METHOD_LEN 8
#define URI_LEN 1024
#define VERSION_LEN 16
#define HOST_LEN 128
#define UA_LEN 32
#define CONT_TYPE 128

extern const char CRLF[];
extern const char CRLF2[];
extern const char host[];
extern const char user_agent[];
extern const char cont_len[];
extern const char cont_type[];
extern const char GET[];
extern const char HEAD[];
extern const char POST[];

extern const char ROOT[];

struct http_req_t {
char method[METHOD_LEN];
char uri[URI_LEN];
char version[VERSION_LEN];
char host[HOST_LEN];
char user_agent[UA_LEN];
int cont_len;
char cont_type[CONT_TYPE];
char *contp;
};

struct buf {
char *buf;
char *sentinal;

char *buf_head;
char *buf_tail;

int size; // tail - head
int free_size; // BUF_SIZE - size

int allocated;


struct http_req_t *http_req_p;
int req_header_received;
int request_received;

};

void init_buf(struct buf *bufp);
void clear_buf(struct buf *bufp);
void reset_buf(struct buf *bufp);
int is_2big(int fd);

#endif
