#ifndef _HELPER_H_
#define _HELPER_H_

#include <stdio.h>
#include <openssl/ssl.h>

#define DEBUG 1
#define dbprintf(...) do{if(DEBUG) fprintf(stderr, __VA_ARGS__); }while(0)

#define MAX_SOCK 1024
#define BUF_SIZE 8192
#define PATH_MAX 1024
#define HEADER_LEN 1024
#define ARG_MAX 1024

extern const char CRLF[];
extern const char CRLF2[];
extern const char host[];
extern const char user_agent[];
extern const char cont_len[];
extern const char cont_type[];
extern const char connection[];
extern const char VERSION[];
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
extern const char CGI[];

extern const char TEXT_HTML[];
extern const char TEXT_CSS[];
extern const char IMAGE_PNG[];
extern const char IMAGE_JPEG[];
extern const char IMAGE_GIF[];

extern const char ROOT[];

extern const int CODE_UNSET;

extern int cgi_fds[];
#define is_cgifd(i) (cgi_fds[(i)])
#define set_cgifd(i) (cgi_fds[(i)] = 1)
#define clear_cgifd(i) (cgi_fds[(i)] = 0)

struct http_req {

    char method[HEADER_LEN];
    char uri[HEADER_LEN];
    char version[HEADER_LEN];
    char host[HEADER_LEN];
    char user_agent[HEADER_LEN];
    int cont_len;
    char cont_type[HEADER_LEN];
    char *contp;
    char connection[HEADER_LEN];

    char *cgi_arg_list[ARG_MAX];
    char *cgi_env_list[ARG_MAX];
    int fds[2];

    struct http_req *next;

};

struct req_queue {

    struct http_req *req_head;
    struct http_req *req_tail;
    int req_count;    

};

struct buf {

    struct req_queue *req_queue_p;

    // reception part
    struct http_req *http_req_p;
    int req_line_header_received;
    int req_body_received;
    int req_fully_received;
    int rbuf_req_count;

    char *rbuf;
    char *rbuf_head;
    char *rbuf_tail;
    char *line_head;
    char *line_tail;
    char *parse_p;
    int rbuf_free_size;
    int rbuf_size;

    // reply part
    struct http_req *http_reply_p;

    char *buf;
    char *buf_head;
    char *buf_tail;
    int buf_size; // tail - head
    int buf_free_size; // BUF_SIZE - size

    int res_line_header_created;
    int res_body_created;
    int res_fully_created;
    int res_fully_sent;

    char *path; // GET/HEAD file path
    //    char path[PATH_MAX];
    long offset; // keep track of read offset

    
    int allocated;

    // ssl part
    SSL *client_context;

    // cgi part
    int port;
    const char *cgiscript;
    

};

void init_buf(struct buf *bufp, const char *cgiscript);
int init_ssl_contex(struct buf *bufp, SSL_CTX *ssl_contex, int sock);
void reset_buf(struct buf *bufp);
void reset_rbuf(struct buf *bufp);
int is_2big(int fd);

int push_str(struct buf* bufp, const char *str);
int push_fd(struct buf* bufp);
void push_error(struct buf *bufp, const char *msg);

void send_error(int sock, const char msg[]);

struct http_req *req_peek(struct req_queue *q);
void req_enqueue(struct req_queue *q, struct http_req *p);
struct http_req *req_dequeue(struct req_queue *q);
void dequeue_request(struct buf *bufp);
void dbprint_queue(struct req_queue *q);

int close_socket(int sock);
int clear_buf(struct buf *bufp);
void clear_buf_array(struct buf *buf_pts[], int maxfd);

int check_path(struct buf *bufp);

void enlist(char *arg_list[], char *arg);
char *delist(char *arg_list[]);
void dbprintf_arglist(char **list);

#endif
