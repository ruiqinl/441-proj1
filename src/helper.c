
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include "helper.h"
#include "http_replyer.h"

const char CRLF[] = "\r\n";
const char CRLF2[] = "\r\n\r\n";
const char host[] = "Host:";
const char user_agent[] = "User-Agent:";
const char cont_len[] = "Content-Length:";
const char cont_type[] = "Content-Type:";
const char connection[] = "Connection:";
const char GET[] = "GET";
const char HEAD[] = "HEAD";
const char POST[] = "POST";
const char VERSION[] = "HTTP/1.1";

const char MSG200[] = "HTTP/1.1 200 OK\r\n";
const char MSG404[] = "HTTP/1.1 404 NOT FOUND\r\n";
const char MSG411[] = "HTTP/1.1 411 LENGTH REQUIRED\r\n";
const char MSG500[] = "HTTP/1.1 500 INTERNAL SERVER ERROR\r\n";
const char MSG501[] = "HTTP/1.1 501 NOT IMPLEMENTED\r\n";
const char MSG503[] = "HTTP/1.1 503 SERCIVE UNAVAILABLE\r\n";
const char MSG505[] = "HTTP/1.1 505 HTTP VERSION NOT SUPPORTED\r\n";

const char TEXT_HTML[] = "text/html";
const char TEXT_CSS[] = "text/css";
const char IMAGE_PNG[] = "image/png";
const char IMAGE_JPEG[] = "image/jpeg";
const char IMAGE_GIF[] = "image/gif";

const char server[] = "Server: Liso/1.0\r\n";

const char ROOT[] = "../static_site";
const int CODE_UNSET = -2;// -2 is not used by parse_request

void init_req_queue(struct req_queue *p) {
    p->req_head = NULL;
    p->req_tail = NULL;
    p->req_count = 0;
}

void init_buf(struct buf* bufp){

    bufp->req_queue_p = (struct req_queue *)calloc(1, sizeof(struct req_queue));
    
    // reception part
    init_req_queue(bufp->req_queue_p);
    bufp->req_line_header_received = 0;
    bufp->req_fully_received = 1; // see parse_request for reason
    
    bufp->http_req_p = bufp->req_queue_p->req_head;

    bufp->rbuf = (char *) calloc(BUF_SIZE, sizeof(char));
    //memset(bufp->rbuf, 0, BUF_SIZE);
    bufp->rbuf_head = bufp->rbuf;
    bufp->rbuf_tail = bufp->rbuf;
    bufp->line_head = bufp->rbuf;
    bufp->line_tail = bufp->rbuf;
    bufp->parse_p = bufp->rbuf;
    bufp->rbuf_free_size = BUF_SIZE;
    bufp->rbuf_size = 0;

    // response part
    bufp->http_reply_p = NULL;

    bufp->buf = (char *) calloc(BUF_SIZE, sizeof(char));    
    //memset(bufp->buf, 0, BUF_SIZE);
    bufp->buf_head = bufp->buf; // p is not used yet in checkpoint-1
    bufp->buf_tail = bufp->buf_head; // empty buffer, off-1 sentinal

    bufp->buf_free_size = BUF_SIZE;
    bufp->buf_size = 0;

    bufp->res_line_header_created = 0;
    bufp->res_body_created = 0;
    bufp->res_fully_created = 0; 
    bufp->res_fully_sent = 1; // see create_response for reason
 
    bufp->path = (char *)calloc(PATH_MAX, sizeof(char)); // file path
    //memset(bufp->path, 0, PATH_MAX);
    bufp->offset = 0; // file offest 

    bufp->allocated = 1;
    bufp->client_context = NULL;

}

int init_ssl_contex(struct buf *bufp, SSL_CTX *ssl_context, int sock) {

    if (bufp->client_context != NULL) 
	fprintf(stderr, "Warning! init_ssl_contex, contex already exists\n");

    if ((bufp->client_context = SSL_new(ssl_context)) == NULL) {
	fprintf(stderr, "Error! init_ssl_contex");
	return -1;
    }
    
    if (SSL_set_fd(bufp->client_context, sock) == 0) {
	fprintf(stderr, "Error! SSL_set_fd\n");
	return -1;
    }

    if (SSL_accept(bufp->client_context) <= 0) {
	fprintf(stderr, "Error! SSL_accept\n");
	return -1;
    }

    return 0;

}

/* returnt the # of bytes pushed into the buffer  */
int push_str(struct buf* bufp, const char *str) {
    int str_size;
    str_size = strlen(str);
    
    if (str_size <= bufp->buf_free_size) {

	strncpy(bufp->buf_tail, str, str_size);

	bufp->buf_tail += str_size;
	bufp->buf_size += str_size;
	bufp->buf_free_size -= str_size;
	
	return str_size;

    } else {
	// deal with it later
	fprintf(stderr, "Warnning! push_str: no enough space in buffer\n");
	return 0;
    }
    
}

/* read from file and push to buffer
 * return 0 when bufp->free_size == 0 or EOF
 * return 1 when reading not finished
 * return -1 on error
 */
int push_fd(struct buf* bufp) {
    int fd;
    long readret;
    
    if (bufp->buf_free_size == 0) {
	dbprintf("Warnning! push_fd: bufp->free_size == 0, wait for sending bytes to free some space\n");
	return 1;
    }


    if ((fd = open(bufp->path, O_RDONLY)) == -1) {
	dbprintf("bufp->path:%s\n", bufp->path);
	perror("Error! push_fd, open");
	return -1;
    }
    
    dbprintf("push_fd: seek to previous position %ld and start reading\n", bufp->offset);
    lseek(fd, bufp->offset, SEEK_SET);

    if ((readret = read(fd, bufp->buf_tail, bufp->buf_free_size)) == -1) {
	perror("Error! push_fd, read");
	close(fd);
	return -1;
    } 

    fprintf(stderr, "push_fd: %ld bytes read this time\n", readret);
    
    bufp->buf_tail += readret;
    bufp->buf_size += readret;
    bufp->buf_free_size -= readret;
    bufp->offset += readret;

    if (readret < bufp->buf_free_size){
	// EOF
	close(fd);
	return 0;

    } else if (readret == bufp->buf_free_size) {
	// send the buffer out, and when the buffer is clear, come back and read again
	close(fd);
	return 1;
    }

    return 1; // this line is not reachable 
}


void reset_buf(struct buf* bufp) {
    if (bufp->allocated == 1) {

	//memset(bufp->buf, 0, BUF_SIZE); // for buf array
	memset(&(bufp->buf[0]), 0, BUF_SIZE); // for buf pointer

	bufp->buf_head = bufp->buf;
	bufp->buf_tail = bufp->buf;

	bufp->buf_size = 0;
	bufp->buf_free_size = BUF_SIZE;

    } else 
	fprintf(stderr, "Warning: reset_buf, buf is not allocated yet\n");
    
}

void reset_rbuf(struct buf *bufp) {
    
    if (bufp->allocated == 1) {
	//memset(bufp->rbuf, 0, BUF_SIZE);
	memset(&(bufp->rbuf[0]), 0, BUF_SIZE);
    
	bufp->rbuf_head = bufp->rbuf;
	bufp->rbuf_tail = bufp->rbuf;
	bufp->line_head = bufp->rbuf;
	bufp->line_tail = bufp->rbuf;
	bufp->parse_p = bufp->rbuf;
    
	bufp->rbuf_size = 0;
	bufp->rbuf_free_size = BUF_SIZE;
    } else 
	fprintf(stderr, "Warnning: reset_rbuf, buf is not allocated yet\n");
    
}



int is_2big(int fd) {
    if (fd >= MAX_SOCK) {
	fprintf(stderr, "Warning! fd %d >= MAX_SOCK %d, it's closed, the client might receive connection reset error\n", fd, MAX_SOCK);
	return 1;
    }
    return 0;
}


void req_enqueue(struct req_queue *q, struct http_req *p) {
    struct http_req *r;

    if (q->req_head != NULL) {
	r = q->req_head;
	while (r->next != NULL)
	    r = r->next;
	r->next = p;
    } else {
	q->req_head = p;
    }
    
    q->req_count += 1;
}

struct http_req *req_dequeue(struct req_queue *q) {
    struct http_req *r;

    if (q->req_head == NULL) {
	fprintf(stderr, "Error! req_dequeue: req_queue is empty\n");
	return NULL;
    }
    r = q->req_head;
    q->req_head = r->next;

    q->req_count -= 1;
    
    return r;
}

void dbprint_queue(struct req_queue *q) {
    struct http_req *p = q->req_head;

    if (p == NULL)
	dbprintf("print_queue:%d requests\nnull\n", q->req_count);

    dbprintf("print_queue:%d requests\n", q->req_count);
    while (p != NULL) {
	dbprintf("request: %s %s %s ...\n", p->method, p->uri, p->version);
	p = p->next;
    }
    

}


void send_error(int sock, const char msg[]) {
    int flag;

    if ((flag = fcntl(sock, F_GETFL, 0)) == -1)
	perror("Error! send_error fcntl F_GETFL");

    flag |= O_NONBLOCK;

    if (fcntl(sock, F_SETFL, flag) == -1)
	perror("Error! send_error fcntl F_SETFL");

    if (send(sock, msg, strlen(msg), 0) == -1)
	perror("Error! send_error send");
        
}

int close_socket(int sock) {
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

int clear_buf(struct buf *bufp){
    
    if (bufp->allocated == 0) {
	fprintf(stderr, "Warnning! clear_buf a buf which is not allocated\n");
	return -1;
    }

    free(bufp->http_req_p);
    free(bufp->req_queue_p);
    free(bufp->rbuf);
    free(bufp->buf);
    free(bufp->path);
    free(bufp->client_context);
    
    bufp->allocated = 0;
    bufp->client_context = NULL;

    return 0;
}

void clear_buf_array(struct buf *buf_pts[], int maxfd){

    int i;

    for (i = 0; i <= maxfd; i++ ) {
	if (buf_pts[i]->allocated == 1) {
	    free(buf_pts[i]);
	    //clear_buf(buf_pts[i]);
	}
    }
}


void push_error(struct buf *bufp, const char *msg) {

    reset_buf(bufp);
    push_str(bufp, msg);
    push_header(bufp);
    push_str(bufp, CRLF);

    bufp->res_line_header_created = 1;
    bufp->res_body_created = 1;
    bufp->res_fully_created = 1;

}
