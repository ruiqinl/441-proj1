#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>

#include "helper.h"
#include "http_parser.h"



/* Return errcode, -1:recv error, 0: recv 0, 1:recv some bytes */
int recv_request(int sock, struct buf *bufp) {

    int readbytes;

    if (bufp->rbuf_free_size == 0)
	dbprintf("Warnning! recv_request, rbuf_free_size == 0\n");

    if ((readbytes = recv(sock, bufp->rbuf_tail, bufp->rbuf_free_size, 0)) <= 0) {
	if (readbytes < 0 )
	    return -1;
	else 
	    return 0;
    }

    // update rbuf
    bufp->rbuf_tail += readbytes; 
    bufp->rbuf_size += readbytes;
    bufp->rbuf_free_size -= readbytes;
    //dbprintf("recv_request:\nwhat recv this time:\n%s\n", bufp->rbuf_tail-readbytes);
    
    return 1;
}


int parse_request(struct buf *bufp) {
    
    char *p;
    
    p = bufp->parse_p;
    //dbprintf("parse_request:\n\trbuf left to parse:\n%s\n", p);

    // count request number based on CRLF2
    while ((p = strstr(p, CRLF2)) != NULL && p < bufp->rbuf_tail) {
	++(bufp->rbuf_req_count);
	p += strlen(CRLF2);
	bufp->parse_p = p;
    }

    // parse every request
    while (bufp->rbuf_req_count != 0) {

	dbprintf("parse_request: request count %d\n", bufp->rbuf_req_count);

	// calloc http_req
	if (bufp->req_fully_received == 1) {
	    //dbprintf("parse_request: start parsing new request\n");
	    bufp->http_req_p = (struct http_req *)calloc(1, sizeof(struct http_req));

	    bufp->req_fully_received = 0;
	    bufp->req_line_header_received = 0;

	}

	// parse req
	parse_request_line(bufp);
	parse_request_headers(bufp);

	bufp->req_line_header_received = 1;// update 

	parse_message_body(bufp); // set req_fully_received inside

	// if fully received, enqueue	
	if (bufp->req_fully_received == 1) {
	    dbprintf("parse_request: req fully received\n");

	    // update req_queue
	    req_enqueue(bufp->req_queue_p, bufp->http_req_p);

	    // update rbuf
	    --(bufp->rbuf_req_count);
	    bufp->rbuf_head = strstr(bufp->rbuf_head, CRLF2) + strlen(CRLF2);

	} else {
	    // only POST can reach here
	    dbprintf("parse_request: POST req not fully received yet, continue receiving\n");
	    break; // break out while loop
	}
	
    }
    
    return 0;
}

int parse_request_line(struct buf *bufp){

    char *p1, *p2;
    struct http_req *http_req_p;
    int len;

    if (bufp->req_line_header_received == 1)
	return 0; // received already, just return

    http_req_p = bufp->http_req_p;
    
    bufp->line_head = bufp->rbuf_head;        
    bufp->line_tail = strstr(bufp->line_head, CRLF); 
   
    // this should never happend, since at least a CRLF2 exists
    if (bufp->line_tail >= bufp->rbuf_tail) {
	dbprintf("request line not found\n");
	return -1;
    }
    
    p1 = bufp->line_head;

    if ((p2 = strchr(p1, ' ')) == NULL || p2 > bufp->line_tail) {
	// cannot find method in this request area
	strcpy(http_req_p->method, "method_not_found");
    } else {
	len = p2 - p1;
	if (len > HEADER_LEN-1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, method buffer overflow\n");
	} 
	strncpy(http_req_p->method, p1, len);
	http_req_p->method[len] = '\0';

	p1 += len + 1;
	dbprintf("http_req->method:%s\n", http_req_p->method);
    }

    
    if ((p2 = strchr(p1, ' ')) == NULL || p2 > bufp->line_tail) {
	// cannot find uri
	strcpy(http_req_p->uri, "uri_not_found");
    } else {
	len = p2 - p1;
	if (len > HEADER_LEN -1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, uri buffer overflow\n");
	}
	strncpy(http_req_p->uri, p1, len);
	http_req_p->uri[len] = '\0';

	p1 += len + 1;
	dbprintf("http_req->uri:%s\n", http_req_p->uri);
    }

    if ((p2 = strstr(p1, CRLF)) == NULL || p2 > bufp->line_tail) {
	// cannot find version
	strcpy(http_req_p->version, "version_not_found");
    } else {
	len = p2 - p1;
	if (len >  HEADER_LEN - 1) {
	    len = HEADER_LEN  - 1;
	    fprintf(stderr, "Warning! parse_request, version buffer overflow\n");
	}
	strncpy(http_req_p->version, p1, len);
	http_req_p->version[len] = '\0';

	p1 += len + strlen(CRLF);
	dbprintf("http_req->version:%s\n", http_req_p->version);
    }

    // update line_head and line_tail, a CRLF always exists since at least a CRLF2 is always there
    bufp->line_head = bufp->line_tail + strlen(CRLF);
    bufp->line_tail = strstr(bufp->line_head, CRLF);;
    if (bufp->line_tail == bufp->line_head)
	dbprintf("Warnning! headers do not exit\n");

    return 0;
}


/* bufp=>line_head and bufp->line_tail have already been updated in method parse_request_line  */
int parse_request_headers(struct buf *bufp) {

    char *p1, *p2;
    struct http_req *http_req_p;
    int tmp_size = 128;
    char tmp[tmp_size];
    int len;

    if (bufp->req_line_header_received == 1)
	return 0; // received already, just return

    http_req_p = bufp->http_req_p;

    p1 = bufp->line_head;
    if ((p2 = strstr(p1, host)) != NULL && p2 < bufp->line_tail) {
	p1 = p2 + strlen(host);
	len = bufp->line_tail - p1;
	if (len > HEADER_LEN - 1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, host buffer overflow\n");
	}
	strncpy(http_req_p->host, p1, len);
	http_req_p->host[len] = '\0';
	dbprintf("http_req->host:%s\n", http_req_p->host);

	// update line_head and head_tail
	bufp->line_head = bufp->line_tail + strlen(CRLF);
	bufp->line_tail = strstr(bufp->line_head, CRLF);
    }

    p1 = bufp->line_head;
    if ((p2 = strstr(p1, user_agent)) != NULL && p2 < bufp->line_tail) {
	p1 = p2 + strlen(user_agent);
	len = bufp->line_tail - p1;
	if (len > HEADER_LEN - 1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_reaquest, user_agent buffer overflow\n");
	}
	strncpy(http_req_p->user_agent, p1, len);
	http_req_p->user_agent[len] = '\0';
	dbprintf("http_req->user_agent:%s\n", http_req_p->user_agent);

	// update line_head and head_tail
	bufp->line_head = bufp->line_tail + strlen(CRLF);
	bufp->line_tail = strstr(bufp->line_head, CRLF);
    }

    p1 = bufp->line_head;
    if ((p2 = strstr(p1, cont_len)) != NULL && p2 < bufp->line_tail) {
	p1 = p2 + strlen(cont_len);
	len = bufp->line_tail - p1;
	if (len > tmp_size-1) {
	    len = tmp_size- 1;
	    fprintf(stderr, "Warning! parse_request, cont_len buffer overflow\n");
	}
	strncpy(tmp, p1, len);
	tmp[len] = '\0';
	http_req_p->cont_len = atoi(tmp);
	dbprintf("http_req->cont_len:%d\n", http_req_p->cont_len);

	// update line_head and head_tail
	bufp->line_head = bufp->line_tail + strlen(CRLF);
	bufp->line_tail = strstr(bufp->line_head, CRLF);
    }
    
    p1 = bufp->line_head;
    if ((p2 = strstr(p1, cont_type)) != NULL && p2 < bufp->line_tail) {
	p1 += strlen(cont_type);
	len = bufp->line_tail - p1;
	if (len >= HEADER_LEN - 1){
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, cont_type buffer overflow\n");
	}
	strncpy(http_req_p->cont_type, p1, len);
	http_req_p->cont_type[len] = '\0';
	dbprintf("http_req->cont_type:%s\n", http_req_p->cont_type);
    }

    // update line_head and head_tail
    bufp->line_head = bufp->line_tail + strlen(CRLF);
    bufp->line_tail = strstr(bufp->line_head, CRLF);
    
    return 0;
}

int parse_message_body(struct buf *bufp) {

    char *p;
    int non_body_size, body_size;
    
    // not POST, receiving request finished
    if (strcmp(bufp->http_req_p->method, POST) != 0) {
	bufp->req_fully_received = 1;
	return 0;
    }

    /* POST does not appear in piped request, so all left bytes are message body */
    if (bufp->http_req_p->cont_len == 0) {
	fprintf(stderr, "Error! POST method does not has Content-Length header\n");
	return -1;
    }

    if ((p = strstr(bufp->rbuf_head, CRLF2)) == NULL) {
	dbprintf("Warnning! parse_message_body, strstr, this line should never be reached\n");
	dbprintf("Remember to send error msg back to client\n");
    }
    p += strlen(CRLF2);

    dbprintf("parse_message_body: body part:%s\n", p);

    non_body_size = p - bufp->rbuf;
    body_size = bufp->rbuf_size - non_body_size;
    if (body_size >= bufp->http_req_p->cont_len)
	bufp->req_fully_received = 1; 
    else 
	bufp->req_fully_received = 0;
	
    return 0;
}

