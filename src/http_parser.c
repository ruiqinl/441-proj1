#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>

#include "helper.h"
#include "http_parser.h"

#define DEBUG 1


/* Return errcode, -1:recv error, 0: recv 0, 1: continue reading, 2: finished reading, 3: error, cannot find Content-Length header(POST), 4: method not found, 5: buffer overflow,  6 service unavailable--set only when accepting too many client, 7 http version not 1.1 msg505  */
int recv_request(int sock, struct buf *bufp) {
    char *p;
    char *crlf_p;
    int readbytes;
    struct http_req_t *http_req_p;

    if (bufp->allocated == 0) {
	fprintf(stderr, "Error! recv_request: bufp not allocated yet\n");
	return 5;
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

    bufp->size += readbytes;
    if (bufp->size > BUF_SIZE) {
	fprintf(stderr, "Error! recv_request: bufp->size > BUF_SIZE\n");
	return 5;
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
    }

    // after received full headers, call parse_request
    dbprintf("recv_request: CRLF2 found\n");
    bufp->req_header_received = 1;
    parse_request(bufp);
    
    dbprintf("recv_request: headers have already been received and parsed\n");

    http_req_p = bufp->http_req_p;

    if (strcmp(http_req_p->version, "HTTP/1.1") != 0) {
	dbprintf("recv_request: version is not HTTP/1.1\n");
	return 7;
    }

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

	    // whenever actual content size is larger than the claimed size in Content-Length, finish reading
	    if ((bufp->buf_tail - p) >= http_req_p->cont_len) {
		bufp->cont_actual_len = bufp->buf_tail - p; 
		bufp->request_received = 1;
		return 2;
	    }
	    return 1;
	}

    } else 
	// find no method
	return 4;
}

/* return 0 on success, 1 on method not found, 2 on uri not found, 3 on version not found   */
int parse_request(struct buf *bufp) {

    char *p1, *p2, *p3;
    int buf_size = 128;
    char buf[buf_size];
    struct http_req_t *http_req = bufp->http_req_p;
    int len;
    
    p1 = bufp->buf;

    dbprintf("parse_request:\n\tparse result:\n");

    if ((p2 = strchr(p1, ' ')) == NULL) {
	// cannot find method
	strcpy(http_req->method, "cannot_parse_method");
    } else {
	len = p2 - p1;
	if (p2-p1 >= METHOD_LEN-1) {
	    len = METHOD_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, method buffer overflow\n");
	} 
	strncpy(http_req->method, p1, len);
	http_req->method[len] = '\0';
	p1 += len + 1;
	dbprintf("http_req->method:%s\n", http_req->method);
    }

    
    if ((p2 = strchr(p1, ' ')) == NULL) {
	// cannot find uri
	strcpy(http_req->uri, "cannot_parse_uri");
    } else {
	len = p2 - p1;
	if (p2 - p1 >= URI_LEN -1) {
	    len = URI_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, uri buffer overflow\n");
	}
	strncpy(http_req->uri, p1, len);
	http_req->uri[len] = '\0';
	p1 += len + 1;
	dbprintf("http_req->uri:%s\n", http_req->uri);
    }

    if ((p2 = strstr(p1, CRLF)) == NULL) {
	// cannot find version
	strcpy(http_req->version, "cannot_parse_version");
    } else {
	len = p2 - p1;
	if (p2 - p1 >= VERSION_LEN - 1) {
	    len = VERSION_LEN  - 1;
	    fprintf(stderr, "Warning! parse_request, version buffer overflow\n");
	}
	strncpy(http_req->version, p1, len);
	http_req->version[len] = '\0';
	p1 += len + strlen(CRLF);
	dbprintf("http_req->version:%s\n", http_req->version);
    }

    if ((p3 = strstr(p1, host)) != NULL) {
	p3 += strlen(host);
	p2 = strstr(p3, CRLF);
	len = p2 - p3;
	if (len >= HOST_LEN - 1) {
	    len = HOST_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, host buffer overflow\n");
	}
	strncpy(http_req->host, p3, len);
	http_req->host[len] = '\0';
	dbprintf("http_req->host:%s\n", http_req->host);
    }
    

    if ((p3 = strstr(p1, user_agent)) != NULL) {
	p3 += strlen(user_agent);
	p2 = strstr(p3, CRLF);
	len = p2 - p3;
	if (len >= UA_LEN - 1) {
	    len = UA_LEN - 1;
	    fprintf(stderr, "Warning! parse_reaquest, user_agent buffer overflow\n");
	}
	strncpy(http_req->user_agent, p3, len);
	http_req->user_agent[len] = '\0';
	dbprintf("http_req->user_agent:%s\n", http_req->user_agent);
    }


    if ((p3 = strstr(p1, cont_len)) != NULL) {
	p3 += strlen(cont_len);
	p2 = strstr(p3, CRLF);
	len = p2 - p3;
	if (len >= buf_size-1) {
	    len = buf_size - 1;
	    fprintf(stderr, "Warning! parse_request, cont_len buffer overflow\n");
	}
	strncpy(buf, p3, len);
	buf[len] = '\0';
	http_req->cont_len = atoi(buf);
	dbprintf("http_req->cont_len:%d\n", http_req->cont_len);
    }
    

    if ((p3 = strstr(p1, cont_type)) != NULL) {
	p3 += strlen(cont_type);
	p2 = strstr(p3, CRLF);
	len = p3 - p2;
	if (len >= CONT_TYPE_LEN - 1){
	    len = CONT_TYPE_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, cont_type buffer overflow\n");
	}
	strncpy(http_req->cont_type, p3, len);
	http_req->cont_type[len] = '\0';
	dbprintf("http_req->cont_type:%s\n", http_req->cont_type);
    }

    // no need to parse body of post, it's handled in recv_request method

    dbprintf("\n");
    return 0;
}


/* return the size of buffer inside struct buf, 0 or a positive number  */ 
int create_response(struct buf *bufp) {
    
    struct http_req_t *http_req_p = bufp->http_req_p;
    int push_ret;

    // headers created, file read done, just send left content in the buffer
    if (bufp->response_created == 1 && bufp->read_done == 1) {
	// full response has been created, just send rest back to client
	dbprintf("create_response: no more content to create\n");
	return bufp->size;
    }

    // headers created, file not read to EOF yet, just continue reading file
    if (bufp->response_created == 1 && bufp->read_done == 0) {
	// headers already sent, just read file
	if ((push_ret = push_fd(bufp)) == 0) {
	    dbprintf("push_fd: file %s is read through\n", bufp->path);
	    bufp->read_done = 1;
	} else if (push_ret == -1) {
	    fprintf(stderr, "Error! Failed reading file, handle this later\n");
	    bufp->read_done = 1;
	} else if (push_ret == 1) {
	    dbprintf("push_ret: file %s is not read through yet\n", bufp->path);
	    bufp->read_done = 0;
	}	
	return bufp->size;
    }
    
    // impossible situation, just for completion
    if (bufp->response_created == 0 && bufp->read_done == 1) {
	fprintf(stderr, "Error! Impossible situation happends: bufp->response_created == 0 && bufp->read_done == 1\n");
	return bufp->size;
    }

    // default case below : if (bufp->response_create == 0 && bufp->read_done == 0)

    // locate the file
    if (bufp->path == NULL) {

	bufp->path = (char *)calloc(PATH_MAX, sizeof(char));
	strcpy(bufp->path, ROOT);
	strcat(bufp->path, http_req_p->uri);

	// locate the file, and check the path is valid
	if (check_path(bufp) == -1) {
	    dbprintf("create_response: check_path failed, send msg404 back");
	    push_str(bufp, MSG404);
	    push_str(bufp, CRLF);
	    push_header(bufp);
	    bufp->response_created = 1; // only this error msg needs to be sent
	    bufp->read_done = 1;
	    return bufp->size;
	}
		
    }

    // check code first
    dbprintf("bufp->code:%d\n", bufp->code);
    if (bufp->code != CODE_UNSET && bufp->code != 2) {
	if (bufp->code == -1 || bufp->code == 5) // recv error, ie 500 internal server error
	    push_str(bufp, MSG500);
	else if (bufp->code == 3) // 411 length required
	    push_str(bufp, MSG411);
	else if (bufp->code == 4) // 501 not implemented
	    push_str(bufp, MSG501);
	else if (bufp->code == 7)
	    push_str(bufp, MSG505);
	// ???how to send MSG503 back????

	push_header(bufp); // anyway, headers are necessary
	bufp->response_created = 1;
	bufp->read_done = 1; // no need to read any file
	return bufp->size;
    }

    // push more response into buffer
    if (strcmp(http_req_p->method, "GET") == 0
	|| strcmp(http_req_p->method, "HEAD") == 0
	|| strcmp(http_req_p->method, "POST") == 0) {

	dbprintf("create_response: create content for %s method\n", http_req_p->method);

	push_str(bufp, MSG200);
	push_header(bufp);
	bufp->response_created = 1;
	
	if (strcmp(http_req_p->method, "GET") == 0){
	    if ((push_ret = push_fd(bufp)) == 0) {
		dbprintf("push_fd: file %s is read through\n", bufp->path);
		//push_str(bufp, CRLF);
		bufp->read_done = 1;
	    } else if (push_ret == -1) {
		fprintf(stderr, "Error! Failed reading file, handle this later\n");
		//push_str(bufp, CRLF);
		bufp->read_done = 1;
	    } else if (push_ret == 1) {
		dbprintf("push_ret: file %s is not read through yet\n", bufp->path);
		bufp->read_done = 0;
	    }
	} else {
	    // HEAD/POST
	    bufp->read_done = 1; // no need to read at all
	}

	// end GET/HEAD/POST
    } else {
	// other methods, not implemented
	// bufp->code == 4; // not use although, just for completion
	push_str(bufp, MSG501);
	push_header(bufp);
	bufp->response_created = 1;
	bufp->read_done = 1;
	dbprintf("create_response: method not implemented\n");
    }
    
    dbprintf("create_response: bufp->size:%d\n", bufp->size);
    return bufp->size;

}

/* return 0 on valid path, -1 on invalid path  */
int check_path(struct buf *bufp) {

    struct stat status;

    if (stat(bufp->path, &status) == -1) {
	perror("Error! check_path stat");		
	return -1;
    }

    if (S_ISDIR(status.st_mode)) {
	strcat(bufp->path, "/index.html");
    }

    // check if path is valid file path
    if (stat(bufp->path, &status) == -1) {
	perror("Error! create_response stat");	
	return -1;
		
    } else if (S_ISREG(status.st_mode)) {
	//	bufp->path = (char *)calloc(strlen(path)+1, sizeof(char));
	//	strcpy(bufp->path, path); // ???check error later???
	dbprintf("check_path: locate file %s\n", bufp->path);
    }
    
    return 0;
}

void push_header(struct buf *bufp){
    char *p;

    if (bufp->headers_created == 0) {

	p = date_str();
	//dbprintf("date_str:%s\n", p);
	push_str(bufp, p); 
	free(p);
	    
	p = connection_str(bufp);
	//	dbprintf("connection_str:%s\n", p);
	push_str(bufp, p);
	free(p);
	    
	//	dbprintf("server:%s\n", server);
	push_str(bufp, server);

	p = cont_len_str(bufp);
	//	dbprintf("cont_len_str:%s\n", p);
	push_str(bufp, p);
	free(p);
	    
	p = cont_type_str(bufp);
	//	dbprintf("cont_type_str:%s\n", p);
	push_str(bufp, p);
	free(p);

	push_str(bufp, CRLF);

	//push_str(bufp, last_modified_str());

	bufp->headers_created = 1;
	dbprintf("create_response: headers pushed into buffer\n");
    }
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

    dbprintf("send_response:%d bytes sent this time\n", sendret);

    bufp->buf_head += sendret;
    bufp->size -= sendret;
    bufp->free_size += sendret;

    // if whole buffer is sent, reset buf for possible un-read part of file
    if (bufp->size == 0) 
	reset_buf(bufp);

    return sendret;
}


char *date_str() {

    time_t rawtime;
    struct tm *timeinfo;
    char *time_str = (char *)calloc(128, sizeof(char));
    
    rawtime = time(NULL);
    timeinfo = gmtime(&rawtime);

    strcpy(time_str, "Date: ");
    strcat(time_str, asctime(timeinfo));
    //strcpy(time_str + strlen("Date: ") + strlen(time_str) - 1, "GMT\r\n");
    //???unable to add GMT???

    return time_str;

}

char *connection_str(struct buf *bufp) {
    
    char *str = (char *)calloc(128, sizeof(char));

    if (strcmp(bufp->http_req_p->connection, "keep-alive") == 0)
	strcpy(str, "Connection: keep-alive\r\n");
    else
	strcpy(str, "Connection: closed\r\n");
    
    return str;
}

char *cont_len_str(struct buf *bufp) {

    struct stat status;
    char *str = (char *)calloc(128, sizeof(char));
    
    if (stat(bufp->path, &status) == -1) {
	perror("Error! cont_len_str, stat");
	strcpy(str, "Content-Length: 0\r\n");
	return str;
    }
    
    sprintf(str, "Content-Length: %lld\r\n", status.st_size);

    return str;
}

char *cont_type_str(struct buf *bufp) {

    char *str = (char *)calloc(128, sizeof(char));
    char *p;

    if ((p = strstr(bufp->path, ".html")) != NULL 
	&& *(p + strlen(".html")) == '\0') {
	sprintf(str, "Content-Type: %s\r\n", TEXT_HTML);
    }

    if ((p = strstr(bufp->path, ".css")) != NULL )
	//	&& *(p + strlen(".css")) == '\0')
	sprintf(str, "Content-Type: %s\r\n", TEXT_CSS);

    if ((p = strstr(bufp->path, ".png")) != NULL 
	&& *(p + strlen(".png")) == '\0')
	sprintf(str, "Content-Type: %s\r\n", IMAGE_PNG);

    if ((p = strstr(bufp->path, ".jpeg")) != NULL 
	&& *(p + strlen(".jpeg")) == '\0')
	sprintf(str, "Content-Type: %s\r\n", IMAGE_JPEG);

    if ((p = strstr(bufp->path, ".gif")) != NULL 
	&& *(p + strlen(".gif")) == '\0')
	sprintf(str, "Content-Type: %s\r\n", IMAGE_GIF);
    
    return str;
}
