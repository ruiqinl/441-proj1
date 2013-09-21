
#include <stdio.h>
#include <stdlib.h>
#include "helper.h"


const char CRLF[] = "\r\n";
const char CRLF2[] = "\r\n\r\n";
const char host[] = "Host:";
const char user_agent[] = "User-Agent:";
const char cont_len[] = "Content-Length:";
const char cont_type[] = "Content-Type:";
const char GET[] = "GET";
const char HEAD[] = "HEAD";
const char POST[] = "POST";

const char ROOT[] = "../static_site";

void init_buf(struct buf* bufp){

    bufp->buf = (char *) calloc(BUF_SIZE, sizeof(char));
    bufp->sentinal = bufp->buf + BUF_SIZE;

    bufp->buf_head = bufp->buf; // p is not used yet in checkpoint-1
    bufp->buf_tail = bufp->buf_head; // empty buffer, off-1 sentinal

    bufp->size = bufp->buf_tail - bufp->buf_head;
    bufp->free_size = BUF_SIZE - (bufp->size);

    bufp->allocated = 1;


    bufp->http_req_p = (struct http_req_t *) calloc(1, sizeof(struct http_req_t));
    bufp->req_header_received = 0;
    bufp->request_received = 0;
    
}

void reset_buf(struct buf* bufp) {
    if (bufp->allocated == 1) {
	//	bufp->p = bufp->buf;
	//	bufp->size = 0;
	bufp->buf_head = bufp->buf;
	bufp->buf_tail = bufp->buf_head;
	// reset http_req ????
    } else 
	fprintf(stderr, "Warning: reset_buf, bufp is not allocated yet\n");
    
}

void clear_buf(struct buf* bufp){

    if (bufp->allocated == 1) {
	free(bufp->buf);
	free(bufp->http_req_p);
    }
    bufp->buf_head = NULL;
    bufp->buf_tail = NULL;
    //    bufp->size = 0;
    
    bufp->allocated = 0;
}


int is_2big(int fd) {
    if (fd >= MAX_SOCK) {
	fprintf(stderr, "Warning! fd %d >= MAX_SOCK %d, it's closed, the client might receive connection reset error\n", fd, MAX_SOCK);
	return 1;
    }
    return 0;
}
