
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "helper.h"

#define DEBUG 1

const char CRLF[] = "\r\n";
const char CRLF2[] = "\r\n\r\n";
const char host[] = "Host:";
const char user_agent[] = "User-Agent:";
const char cont_len[] = "Content-Length:";
const char cont_type[] = "Content-Type:";
const char connec[] = "Connection:";
const char GET[] = "GET";
const char HEAD[] = "HEAD";
const char POST[] = "POST";

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

void init_buf(struct buf* bufp){

    bufp->buf = (char *) calloc(BUF_SIZE, sizeof(char));
    bufp->sentinal = bufp->buf + BUF_SIZE;

    bufp->buf_head = bufp->buf; // p is not used yet in checkpoint-1
    bufp->buf_tail = bufp->buf_head; // empty buffer, off-1 sentinal

    bufp->size = 0;
    bufp->free_size = BUF_SIZE;

    bufp->allocated = 1;


    bufp->http_req_p = (struct http_req_t *) calloc(1, sizeof(struct http_req_t));
    bufp->req_header_received = 0;
    bufp->request_received = 0;
    bufp->headers_created = 0;
    bufp->response_created = 0;
    bufp->read_done = 0;

    bufp->code = CODE_UNSET; // the code returned by parse_request

    bufp->path = NULL; // file path
    bufp->offset = 0; // file offest 

}

/* returnt the # of bytes pushed into the buffer  */
int push_str(struct buf* bufp, const char *str) {
    int str_size;
    str_size = strlen(str);
    
    if (str_size <= bufp->free_size-1) {

	strcat(bufp->buf_tail, str);

	bufp->buf_tail += str_size;
	bufp->size += str_size;
	bufp->free_size -= str_size;
	
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

    
    if (bufp->free_size == 0) {
	dbprintf("Warnning! push_fd: bufp->free_size == 0, wait for sending bytes\n");
	return 1;
    }


    if ((fd = open(bufp->path, O_RDONLY)) == -1) {
	perror("Error! push_fd, open");
	return -1;
    }
    
    dbprintf("push_fd: seek to previous position %ld and start reading\n", bufp->offset);
    lseek(fd, bufp->offset, SEEK_SET);

    if ((readret = read(fd, bufp->buf_tail, bufp->free_size)) == -1) {
	perror("Error! push_fd, read");
	close(fd);
	return -1;
    } 

    fprintf(stderr, "push_fd: %ld bytes read this time\n", readret);
    
    bufp->buf_tail += readret;
    bufp->size += readret;
    bufp->free_size -= readret;
    bufp->offset += readret;

    if (readret < bufp->free_size){
	// eof
	close(fd);
	return 0;

    } else if (readret == bufp->free_size) {
	// send the buffer out, and when the buffer is clear, come back and read again
	close(fd);
	return 1;
    }

    return 1; // continue reading
}


void reset_buf(struct buf* bufp) {
    if (bufp->allocated == 1) {

	bufp->buf_head = bufp->buf;
	bufp->buf_tail = bufp->buf;
	// improve memset part later, BUF_SIZE is a too large number
	// keep track of buf_size inside recv_request method
	memset(bufp->buf_head, 0, BUF_SIZE); 

	bufp->size = 0;
	bufp->free_size = BUF_SIZE;
	
	//	bufp->req_header_received = 0;
	//	bufp->request_received = 0;
	//	bufp->headers_created = 0;
	//	bufp->response_created = 0;
	//	bufp->read_done = 0;

	//bufp->code = -2; 

	//free(bufp->path);
	//bufp->path = NULL;
	//bufp->offset = 0; 

    } else 
	fprintf(stderr, "Warning: reset_buf, buf is not allocated yet\n");
    
}

void clear_buf(struct buf* bufp){

    if (bufp->allocated == 1) {
	free(bufp->buf);
	free(bufp->http_req_p);
	free(bufp->path);
    }
    bufp->buf_head = NULL;
    bufp->buf_tail = NULL;
    bufp->path = NULL;
    
    bufp->allocated = 0;
}


int is_2big(int fd) {
    if (fd >= MAX_SOCK) {
	fprintf(stderr, "Warning! fd %d >= MAX_SOCK %d, it's closed, the client might receive connection reset error\n", fd, MAX_SOCK);
	return 1;
    }
    return 0;
}
