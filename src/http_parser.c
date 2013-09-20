#include <string.h>
#include <stdlib.h>

#include "helper.h"

#define DEBUG 1

int parse_req(struct buf *bufp, struct http_req_t *http_req) {

    //struct http_req_t http_req;

    char *p1, *p2, *p3;
    char buf[8];
    
    p1 = bufp->buf;
    dbprintf("http_parser: buf:\n%s\n", p1);

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
    
    return 0;
}
