#ifndef _HELPER_H_
#define _HELPER_H_

#include <stdio.h>

#define dbprintf(...) do{if(DEBUG) fprintf(stderr, __VA_ARGS__); }while(0)

#define MAX_SOCK 1024
#define BUF_SIZE 8192
#define PATH_MAX 1024

#define HEADER_LEN 1024

extern const char CRLF[];
extern const char CRLF2[];
extern const char host[];
extern const char user_agent[];
extern const char cont_len[];
extern const char cont_type[];
extern const char connec[];
extern const char GET[];
extern const char HEAD[];
extern const char POST[];

extern const char MSG200[];
extern const char MSG404[];
extern const char MSG411[];
extern const char MSG500[];
extern const char MSG501[];
extern const char MSG503[];
extern const char MSG505[];

extern const char server[];

extern const char TEXT_HTML[];
extern const char TEXT_CSS[];
extern const char IMAGE_PNG[];
extern const char IMAGE_JPEG[];
extern const char IMAGE_GIF[];

extern const char ROOT[];

extern const int CODE_UNSET;

struct http_req_t {

    char method[HEADER_LEN];
    char uri[HEADER_LEN];
    char version[HEADER_LEN];
    char host[HEADER_LEN];
    char user_agent[HEADER_LEN];
    int cont_len;
    char cont_type[HEADER_LEN];
    char *contp;
    char connection[HEADER_LEN];

};


struct buf {

    char *buf;
    char *sentinal;

    char *buf_head;
    char *buf_tail;

    int size; // tail - head
    int free_size; // BUF_SIZE - size
    int cont_actual_len; // the actual size of body in POST

    int allocated;


    struct http_req_t *http_req_p;
    int req_header_received; // if headers are all received
    int request_received; // indicate if request is fully received
    int headers_created; // if header of response is fully created
    int response_created; // indicate if response including headers is fully created
int read_done;// indicate if file has been read throghtly

    int code; // save the result of parse_request

    char *path; // GET/HEAD file path
    long offset; // keep track of read offset

};

void init_buf(struct buf *bufp);
void clear_buf(struct buf *bufp);
void reset_buf(struct buf *bufp);
int is_2big(int fd);

int push_str(struct buf* bufp, const char *str);
int push_fd(struct buf* bufp);


#endif
