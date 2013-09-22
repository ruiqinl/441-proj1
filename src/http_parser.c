#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

#include "helper.h"
#include "http_parser.h"

#define DEBUG 1
#define PATH_MAX 1024



/* Return errcode, -1:recv error, 0: recv 0, 1: continue reading, 2: finished reading, 3: error, cannot find Content-Length header(POST), 4: other err  */
int recv_request(int sock, struct buf *bufp) {
    char *p;
    char *crlf_p;
    int readbytes;
    struct http_req_t *http_req_p;

    if (bufp->allocated == 0) {
	fprintf(stderr, "Error! recv_request: bufp not allocated yet\n");
	return 4;
    }

    if (bufp->request_received == 1) 
	// request already received
	return 2;

    // request not fully received yet
    
    if ((readbytes = recv(sock, bufp->buf_tail, bufp->free_size, 0)) <= 0) {
	// return errcode
	if (readbytes < 0 )
	    return -1;
	else 
	    return 0;
    }
    bufp->buf_tail += readbytes; // update tail
    dbprintf("recv_request:\n\twhat recv this time:\n%s\n", bufp->buf_tail-readbytes);

    // headers not fully received yet
    if (bufp->req_header_received == 0) {

	if ((crlf_p = strstr(bufp->buf, CRLF2)) == NULL) {
	    // continue reading, return 1
	    dbprintf("recv_request: CRLF2 not found\n");
	    return 1;
	}

	// after received full headers, call parse_request
	dbprintf("recv_request: CRLF2 found\n");
	bufp->req_header_received = 1;
	parse_request(bufp);
    }
    
    // headers received and parsed already
    // check request integrity based on methods
    dbprintf("recv_request: headers have already been received and parsed\n");

    http_req_p = bufp->http_req_p;

    if (strcmp(http_req_p->method, GET) == 0
	|| strcmp(http_req_p->method, HEAD) == 0) {
	// GET/HEAD
	dbprintf("recv_request: method is GET/HEAD, request fully received\n");
        bufp->request_received = 1;
	return 2;

    } else if (strcmp(http_req_p->method, POST) == 0) {
	// POST, check Content-Length
	dbprintf("recv_request: method is POST, check Content-Length\n");

	if (http_req_p->cont_len == 0) {
	    dbprintf("recv_request: cannot find Content-Length\n");
	    return 3;

	} else { 
	    // check Content-Length with length of content
	    crlf_p = strstr(bufp->buf, CRLF2); // CRLF2 must be there
	    p = crlf_p + strlen(CRLF2);

	    dbprintf("recv_request: in the data area of post:%s\n", p);
	    dbprintf("recv_request: strlen(p):%ld ?>= http_req_p->cont_len:%d\n",strlen(p),http_req_p->cont_len);

	    if (strlen(p) >= http_req_p->cont_len) {// ??? can we use strlen ???
		bufp->request_received = 1;
		return 2;
	    }
	    return 1;
	}

    } else 
	// find no method, error
	return 4;
}

int parse_request(struct buf *bufp) {

    char *p1, *p2, *p3;
    char buf[8];
    struct http_req_t *http_req = bufp->http_req_p;
    
    p1 = bufp->buf;
    dbprintf("parse_request:\n\twhole buf received:\n%s\n", p1);
    dbprintf("parse_request:\n\tparse result:\n");

    p2 = strchr(p1, ' ');
    strncpy(http_req->method, p1, p2-p1);
    http_req->method[p2-p1] = '\0';
    p1 = p2 + 1;
    dbprintf("http_req->method:%s\n", http_req->method);
    
    p2 = strchr(p1, ' ');
    strncpy(http_req->uri, p1, p2-p1);
    http_req->uri[p2-p1] = '\0';
    p1 = p2 + 1;
    dbprintf("http_req->uri:%s\n", http_req->uri);

    p2 = strstr(p1, CRLF);
    strncpy(http_req->version, p1, p2-p1);
    http_req->version[p2-p1] = '\0';
    p1 = p2 + strlen(CRLF);
    dbprintf("http_req->version:%s\n", http_req->version);

    p3 = strstr(p1, host);
    if (p3 != NULL) {
	p3 += strlen(host);
	p2 = strstr(p3, CRLF);
	strncpy(http_req->host, p3, p2-p3);
	dbprintf("http_req->host:%s\n", http_req->host);
    }
    
    
    p3 = strstr(p1, user_agent);
    if (p3 != NULL) {
	p3 += strlen(user_agent);
	p2 = strstr(p3, CRLF);
	strncpy(http_req->user_agent, p3, p2-p3);
	http_req->user_agent[p2-p3] = '\0';
	dbprintf("http_req->user_agent:%s\n", http_req->user_agent);
    }
    

    p3 = strstr(p1, cont_len);
    if (p3 != NULL) {
	p3 += strlen(cont_len);
	p2 = strstr(p3, CRLF);
	strncpy(buf, p3, p2-p3);
	buf[p2-p3] = '\0';
	http_req->cont_len = atoi(buf);
	dbprintf("http_req->cont_len:%d\n", http_req->cont_len);
    }
    

    p3 = strstr(p1, cont_type);
    if (p3 != NULL) {
	p3 += strlen(cont_type);
	p2 = strstr(p3, CRLF);
	strncpy(http_req->cont_type, p3, p2-p3);
	http_req->cont_type[p2-p3] = '\0';
	dbprintf("http_req->cont_type:%s\n", http_req->cont_type);
    }
    
    dbprintf("\n");
    return 0;
}


/* return the size of buffer inside struct buf, 0 or a positive number  */ 
int create_response(struct buf *bufp) {
    
    int fd;
    struct stat status;
    char path[PATH_MAX];

    struct http_req_t *http_req_p = bufp->http_req_p;

    if (bufp->response_created == 1) {
	// full response has been created, just send rest back to client
	dbprintf("create_response: no more content to create\n");
	return bufp->size;
    }

    // push more response into buffer
    if (strcmp(http_req_p->method, "GET") == 0) {
	dbprintf("create_response: create content for GET method\n");

	// never located this file before
	if (bufp->path == NULL) {

	    strcpy(path, ROOT);
	    strcat(path, http_req_p->uri);

	    // check if path is valid
	    if (stat(path, &status) == -1) {
		perror("Error! create_response stat, send 404 back");
		push_str(bufp, msg404);

		bufp->response_created = 1; // only this error msg needs to be sent		
		return bufp->size;
	    }

	    if (S_ISDIR(status.st_mode)) {
		strcat(path, "/index.html");
	    }

	    // check if path is valid file path
	    if (stat(path, &status) == -1) {
		perror("Error! create_response stat, send 404 back");
		push_str(bufp, msg404);

		bufp->response_created = 1; // only this error msg needs to be sent	
		return bufp->size;
		
	    } else if (S_ISREG(status.st_mode)) {
		bufp->path = (char *)calloc(strlen(path)+1, sizeof(char));
		strcpy(bufp->path, path); // ???check error later???
		dbprintf("create_response: locate file %s\n", bufp->path);
	    }
	}

	// file located just now/before

	if (bufp->headers_created == 0) {
	    push_str(bufp, msg200);
	    bufp->headers_created = 1;
	    dbprintf("create_response: headers pushed into buffer\n");
	}

	if (push_fd(bufp) == 0)
	    bufp->response_created = 1; // bufp->free_size == 0 OR EOF
	close(fd);

    }
    
    return bufp->size;

}

/* return 0 if buf->size == 0  */
int send_response(int sock, struct buf *bufp) {
    int sendret;

    if (bufp->size == 0) {
	dbprintf("send_response: buffer is empty, sending finished\n");
	return 0;
    }
    
    if ((sendret = send(sock, bufp->buf_head, bufp->size, 0)) == -1) {
	perror("Error! send_response: send");
	return -1;
    }

    bufp->buf_head += sendret;
    bufp->size -= sendret;
    // free_size ? if cicular buffer is implemented

    return sendret;
}

