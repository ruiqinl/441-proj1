#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>
#include <ctype.h>

#include "helper.h"
#include "http_parser.h"


/* Return errcode, -1:recv error, 0: recv 0, 1:recv some bytes */
int recv_request(int sock, struct buf *bufp) {

    int readbytes;

    if (bufp->req_fully_received == 1) {
	reset_rbuf(bufp);
	dbprintf("recv_request: fully_received==1, reset rbuf\n");
    }
    
    // if free_size == 0, it confuses lisod to think that the client socket is closed
    if (bufp->rbuf_free_size == 0) {
    	dbprintf("Warnning! recv_request, rbuf_free_size == 0, send MSG503 back\n");
	send_error(sock, MSG503); // non_block
	return 0;
    }


    if (bufp->client_context != NULL) 
	// ssl socket
	readbytes = SSL_read(bufp->client_context, bufp->rbuf_tail, bufp->rbuf_free_size);
    else 
	// non-ssl socket
	readbytes = recv(sock, bufp->rbuf_tail, bufp->rbuf_free_size, 0);

    // update rbuf
    bufp->rbuf_tail += readbytes; 
    bufp->rbuf_size += readbytes;
    bufp->rbuf_free_size -= readbytes;
    //dbprintf("recv_request:\nwhat recv this time:\n%s\n", bufp->rbuf_tail-readbytes);

    if (readbytes < 0 )
	return -1;
    else if (readbytes == 0)
	return 0;
    return 1;
}


int parse_request(struct buf *bufp) {
    
    char *p;
    
    p = bufp->parse_p;
    dbprintf("parse_request:\n\trbuf left to parse:\n%s\n", p);

    // count request number based on CRLF2
    while ((p = strstr(p, CRLF2)) != NULL && p < bufp->rbuf_tail) {
	++(bufp->rbuf_req_count);
	p += strlen(CRLF2);
	bufp->parse_p = p;
    }

    if (bufp->rbuf_req_count == 0)
	bufp->req_fully_received = 0;

    // parse every possible request in the rbuf
    while (bufp->rbuf_req_count != 0) {

	dbprintf("parse_request: request count %d\n", bufp->rbuf_req_count);

	// calloc http_req
	bufp->http_req_p = (struct http_req *)calloc(1, sizeof(struct http_req));
	bufp->req_fully_received = 0;
	bufp->req_line_header_received = 0;

	// parse req
	parse_request_line(bufp);
	parse_request_headers(bufp);

	bufp->req_line_header_received = 1;// update 

	 // set req_fully_received inside
	if (parse_message_body(bufp) == -1) {
	    // POST has no Content-Length header
	    push_error(bufp, MSG411);
	}
	    

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
   
    if (bufp->line_tail >= bufp->rbuf_tail) {
	// this should never happend, since at least a CRLF2 exists
	dbprintf("request line not found\n");
	return -1;
    }
    

    p1 = bufp->line_head;
    while (isspace(*p1))
	p1++;

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
	while (isspace(*p1))
	    p1++;

    }
    dbprintf("http_req->method:%s\n", http_req_p->method);

    
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
	while (isspace(*p1))
	    p1++;
    }
    dbprintf("http_req->uri:%s\n", http_req_p->uri);
    
    if ((p2 = strstr(p1, CRLF)) == NULL || p2 > bufp->line_tail) {
	// cannot find version
	strcpy(http_req_p->version, "version_not_found");
    } else {

	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	if (len >  HEADER_LEN - 1) {
	    len = HEADER_LEN  - 1;
	    fprintf(stderr, "Warning! parse_request, version buffer overflow\n");
	}
	strncpy(http_req_p->version, p1, len);
	http_req_p->version[len] = '\0';

	p1 += len + strlen(CRLF);
    }
    dbprintf("http_req->version:%s\n", http_req_p->version);

    // update line_head and line_tail, a CRLF always exists since at least a CRLF2 is always there
    // now line_head and line_tail
    bufp->line_head = bufp->line_tail + strlen(CRLF);
    bufp->line_tail = strstr(bufp->line_head, CRLF);
    if (bufp->line_tail == bufp->line_head)
	dbprintf("Warnning! headers do not exist\n");
    else {
	// headers exist, put line_tail to end of headers
	bufp->line_tail = strstr(bufp->line_head, CRLF2);
    }

    return 0;
}


/* bufp->line_head and bufp->line_tail have already been updated in method parse_request_line  */
int parse_request_headers(struct buf *bufp) {

    char *p1, *p2;
    struct http_req *http_req_p;
    int tmp_size = 128;
    char tmp[tmp_size];
    int len;

    if (bufp->req_line_header_received == 1)
	return 0; // received already, just return

    http_req_p = bufp->http_req_p;

    if ((p1 = strstr(bufp->line_head, cont_type)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(cont_type);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	
	if (len >= HEADER_LEN - 1){
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, cont_type buffer overflow\n");
	}
	strncpy(http_req_p->cont_type, p1, len);
	http_req_p->cont_type[len] = '\0';
    } else 
	strcat(http_req_p->cont_type, "");
    dbprintf("http_req->cont_type:%s\n", http_req_p->cont_type);

    if ((p1 = strstr(bufp->line_head, accept)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(accept);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	
	if (len >= HEADER_LEN - 1){
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, accept buffer overflow\n");
	}
	strncpy(http_req_p->http_accept, p1, len);
	http_req_p->http_accept[len] = '\0';
    } else 
	strcat(http_req_p->http_accept, "");
    dbprintf("http_req->http_accept:%s\n", http_req_p->http_accept);

     if ((p1 = strstr(bufp->line_head, referer)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(referer);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	
	if (len >= HEADER_LEN - 1){
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, referer buffer overflow\n");
	}
	strncpy(http_req_p->http_referer, p1, len);
	http_req_p->http_referer[len] = '\0';
    } else 
	strcat(http_req_p->http_referer, "");
    dbprintf("http_req->http_referer:%s\n", http_req_p->http_referer);


    if ((p1 = strstr(bufp->line_head, host)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(host);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	if (len > HEADER_LEN - 1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, host buffer overflow\n");
	}
	strncpy(http_req_p->host, p1, len);
	http_req_p->host[len] = '\0';
	
    } else 
	strcat(http_req_p->host, "");
    dbprintf("http_req->host:%s\n", http_req_p->host);

    if ((p1 = strstr(bufp->line_head, encoding)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(encoding);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	if (len > HEADER_LEN - 1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, host buffer overflow\n");
	}
	strncpy(http_req_p->http_accept_encoding, p1, len);
	http_req_p->http_accept_encoding[len] = '\0';
	
    } else 
	strcat(http_req_p->http_accept_encoding, "");
    dbprintf("http_req->http_accept_encoding:%s\n", http_req_p->http_accept_encoding);

    if ((p1 = strstr(bufp->line_head, language)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(language);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	if (len > HEADER_LEN - 1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, host buffer overflow\n");
	}
	strncpy(http_req_p->http_accept_language, p1, len);
	http_req_p->http_accept_language[len] = '\0';
	
    } else 
	strcat(http_req_p->http_accept_language, "");
    dbprintf("http_req->http_accept_language:%s\n", http_req_p->http_accept_language);

    if ((p1 = strstr(bufp->line_head, charset)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(charset);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	if (len > HEADER_LEN - 1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, host buffer overflow\n");
	}
	strncpy(http_req_p->http_accept_charset, p1, len);
	http_req_p->http_accept_charset[len] = '\0';
	
    } else 
	strcat(http_req_p->http_accept_charset, "");
    dbprintf("http_req->http_accept_charset:%s\n", http_req_p->http_accept_charset);

    if ((p1 = strstr(bufp->line_head, cookie)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(cookie);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	
	if (len >= HEADER_LEN - 1){
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, cookie buffer overflow\n");
	}
	strncpy(http_req_p->cookie, p1, len);
	http_req_p->cookie[len] = '\0';
    } else 
	strcat(http_req_p->cookie, "");
    dbprintf("http_req->cookie:%s\n", http_req_p->cookie);

    if ((p1 = strstr(bufp->line_head, user_agent)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(user_agent);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	if (len > HEADER_LEN - 1) {
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_reaquest, user_agent buffer overflow\n");
	}
	strncpy(http_req_p->user_agent, p1, len);
	http_req_p->user_agent[len] = '\0';
	
    } else 
	strcat(http_req_p->user_agent, "");
    dbprintf("http_req->user_agent:%s\n", http_req_p->user_agent);

    if ((p1 = strstr(bufp->line_head, connection)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(connection);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;
	
	if (len >= HEADER_LEN - 1){
	    len = HEADER_LEN - 1;
	    fprintf(stderr, "Warning! parse_request, cont_type buffer overflow\n");
	}
	strncpy(http_req_p->connection, p1, len);
	http_req_p->connection[len] = '\0';
    } else 
	strcat(http_req_p->connection, "");
    dbprintf("http_req->connection:%s\n", http_req_p->connection);


    if ((p1 = strstr(bufp->line_head, cont_len)) != NULL && p1 < bufp->line_tail) {

	p1 += strlen(cont_len);
	while (isspace(*p1))
	    ++p1;

	p2 = strstr(p1, CRLF);
	while (isspace(*(p2-1)))
	    --p2;

	len = p2 - p1;


	//len = bufp->line_tail - p1;
	if (len > tmp_size-1) {
	    len = tmp_size- 1;
	    fprintf(stderr, "Warning! parse_request, cont_len buffer overflow\n");
	}
	strncpy(tmp, p1, len);
	tmp[len] = '\0';
	http_req_p->cont_len = atoi(tmp);
	

    }
    dbprintf("http_req->cont_len:%d\n", http_req_p->cont_len);

    

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
	bufp->req_fully_received = 1;
	return -1;
    }

    if ((p = strstr(bufp->rbuf_head, CRLF2)) == NULL) {
	dbprintf("Warnning! parse_message_body, strstr, this line should never be reached\n");
	dbprintf("Remember to send error msg back to client\n");
    }
    p += strlen(CRLF2);
    
    bufp->http_req_p->contp = (char *)calloc(bufp->http_req_p->cont_len+1, sizeof(char));
    strcpy(bufp->http_req_p->contp, p);

    dbprintf("parse_message_body: body part:%s\n", bufp->http_req_p->contp);

    non_body_size = p - bufp->rbuf;
    body_size = bufp->rbuf_size - non_body_size;
    if (body_size >= bufp->http_req_p->cont_len)
	bufp->req_fully_received = 1; 
    else 
	bufp->req_fully_received = 0;
	
    return 0;
}

