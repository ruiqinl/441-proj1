#include <sys/stat.h>
#include <sys/select.h>
#include <unistd.h>
#include "cgi_parser.h"
#include "helper.h"

const char CONTENT_LENGTH[] = "CONTENT_LENGTH=";
const char CONTENT_TYPE[] = "CONTENT_TYPE=";
const char GATEWAY_INTERFACE[] = "GATEWAY_INTERFACE=CGI/1.1";
const char PATH_INFO[] = "PATH_INFO=";
const char QUERY_STRING[] = "QUERY_STRING=";
const char REMOTE_ADDR[] = "REMOTE_ADDR=";
const char REQUEST_METHOD[] = "REQUEST_METHOD=";
const char REQUEST_URI[] = "REQUEST_URI=";
const char SCRIPT_NAME[] = "SCRIPT_NAME=/cgi";
const char SERVER_PORT[] = "SERVER_PORT=";
const char SERVER_PROTOCOL[] = "SERVER_PROTOCOL=HTTP/1.1";
const char SERVER_SOFTWARE[] = "SERVER_SOFTWARE=Liso/1.0";
const char HTTP_ACCEPT[] = "HTTP_ACCEPT=";
const char HTTP_REFERER[] = "HTTP_REFERER=";
const char HTTP_ACCEPT_ENCODING[] = "HTTP_ACCEPT_ENCODING=";
const char HTTP_ACCEPT_LANGUAGE[] = "HTTP_ACCEPT_LANGUAGE=";
const char HTTP_ACCEPT_CHARSET[] = "HTTP_ACCEPT_CHARSET=";
const char HTTP_HOST[] = "HTTP_HOST=";
const char HTTP_COOKIE[] = "HTTP_COOKIE=";
const char HTTP_USER_AGENT[] = "HTTP_USER_AGENT=";
const char HTTP_CONNECTION[] = "HTTP_CONNECTION=";
const char HTTPS[] = "HTTPS=";


const char SERVER_NAME[] = "SERVER_NAME=ruiqinl.no-ip.biz";



int run_cgi (struct buf *bufp){
    
    int cgi_inpipe[2];
    int cgi_outpipe[2]; 
    struct http_req *req_p;
    int pid;
    
    req_p = bufp->http_reply_p;

    if (pipe(cgi_inpipe) == -1) {
	perror("Error! run_cgi, inpipe");
	return -1;
    }

    if (pipe(cgi_outpipe) == -1) {
	perror("Error! run_cgi, outpipe");
	return -1;
    }

    
    
    if((pid = fork()) == -1) {
	perror("Error! run_cgi, fork");
	return -1;
    }


    if (pid == 0) {
	// child process

	close(fileno(stdin));
	close(fileno(stdout));

	dup2(cgi_outpipe[1], fileno(stdout));
	dup2(cgi_inpipe[0], fileno(stdin));

	close(cgi_outpipe[0]);
	close(cgi_outpipe[1]);
	close(cgi_inpipe[0]);
	close(cgi_inpipe[1]);
	
	dbprintf("run_cgi: child process, execve cgi %s\n", bufp->path);
	execve(bufp->path, req_p->cgi_arg_list, req_p->cgi_env_list);
	
	// execve returns, error
	perror("Error! run_cgi, execve");
	return -1;
    } 
    
    //parent process
    // keep record of fds 
    req_p->fds[0] = cgi_outpipe[0];
    req_p->fds[1] = cgi_inpipe[1];
    close(cgi_outpipe[1]);
    close(cgi_inpipe[0]);

    //char tmp[1024];
    //read(cgi_outpipe[0], tmp, 1024);
    //dbprintf("cgi_parser: read from %d:%s\n", cgi_outpipe[0], tmp);fflush(stdout);

    //dbprintf("cgi_parser: waht the heck!!!!\n");fflush(stdout);
    //write(cgi_inpipe[1], "DamnIT!!!", strlen("DanmIT!!!"));
    //close(cgi_inpipe[1]);
    //dbprintf("cgi_parser: waht the heck22222!!!!\n");fflush(stdout);
    //read(cgi_outpipe[0], tmp, 1024);
    //dbprintf("cgi_parser: tmp:%s\n", tmp);

    // keep track of cgi fds
    pipe_buf_array[cgi_outpipe[0]] = bufp;
    pipe_buf_array[cgi_inpipe[1]] = bufp;
    
    return 0;
}

int locate_cgi(struct buf *bufp) {
    struct http_req *req_p;
    char buf[PATH_MAX];
    struct stat status;
    
    req_p = bufp->http_req_p;

    //strcpy(buf, ROOT);
    //strcat(buf, CGI);
    //strcat(buf, bufp->cgiscript);
    strcpy(buf, bufp->cgiscript);
    dbprintf("locate_cgi_file:%s\n", buf);
    
    if (stat(buf,&status) == -1 || S_ISREG(status.st_mode) == -1) {
	dbprintf("Error! locate_cgi_file, stat\n");
	return -1;
    }
    
    strcpy(bufp->path, buf);
    return 0;
}


int parse_cgi_uri(struct buf *bufp) {
    char *p1, *p2;
    struct http_req *req_p;
    char buf[BUF_SIZE];

    req_p = bufp->http_reply_p;

    enlist(req_p->cgi_arg_list, bufp->cgiscript); // ????
    
    // env_list: path, query string
    if((p1 = strstr(req_p->uri, CGI)) == NULL) {
	fprintf(stderr, "Error! parse_cgi_uri, cannot find sub-string /cgi");
	return -1;
    }
    p1 += strlen(CGI);
    
    if ((p2 = strchr(p1, '?')) == NULL) {
	dbprintf("parse_cgi_uri: ? not found, query string is empty\n");

	// path_info
	memset(buf, 0, BUF_SIZE);
	strcpy(buf, PATH_INFO);
	strcat(buf, p1);
	enlist(req_p->cgi_env_list, buf);

	//req_uri
	memset(buf, 0, BUF_SIZE);
	strcpy(buf, REQUEST_URI);
	strcat(buf, req_p->uri);
	enlist(req_p->cgi_env_list, buf);

	// query string, empty
	memset(buf, 0, BUF_SIZE);
	strcpy(buf, QUERY_STRING);
	enlist(req_p->cgi_env_list, buf);
    } else {
	dbprintf("parse_cgi_uri: ? found, query string is not empty\n");
	
	//path_info
	memset(buf, 0, BUF_SIZE);
	strcpy(buf, PATH_INFO);
	strncat(buf, p1, p2-p1);
	enlist(req_p->cgi_env_list, buf);

	// request_uri
	memset(buf, 0, BUF_SIZE);
	strcpy(buf, REQUEST_URI);
	strncat(buf, req_p->uri, p2 - req_p->uri);
	enlist(req_p->cgi_env_list, buf);

	// query string
	memset(buf, 0, BUF_SIZE);
	strcpy(buf, QUERY_STRING);
	strcat(buf, p2+1);
	enlist(req_p->cgi_env_list, buf);

    }

    //env_list: content length
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, CONTENT_LENGTH);
    if (strcmp(req_p->method, POST) == 0) {
	sprintf(buf+strlen(CONTENT_LENGTH), "%d", req_p->cont_len);
    }
    enlist(req_p->cgi_env_list, buf);

    //evn_list: content type
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, CONTENT_TYPE);
    strcat(buf, req_p->cont_type);
    enlist(req_p->cgi_env_list, buf);

    //env_list: gateway
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, GATEWAY_INTERFACE);//CGI/1.1
    enlist(req_p->cgi_env_list, buf);

    // remote_addr
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, REMOTE_ADDR);
    strcat(buf, bufp->remote_addr);
    enlist(req_p->cgi_env_list, buf);

    //env_list: method
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, REQUEST_METHOD);
    strcat(buf, req_p->method);
    enlist(req_p->cgi_env_list, buf);

    // uri already set above

    //env_list: script name
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, SCRIPT_NAME);
    enlist(req_p->cgi_env_list, buf);


    //env_list: server_port
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, SERVER_PORT);
    sprintf(buf+strlen(SERVER_PORT), "%d", bufp->server_port);// ?????
    //sprintf(buf+strlen(SERVER_PORT), "%d", 9999); // add to buf later
    enlist(req_p->cgi_env_list, buf);

    //env_list: server_protocol
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, SERVER_PROTOCOL);// HTTP/1.1
    enlist(req_p->cgi_env_list, buf);

    //env_list: serve_software
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, SERVER_SOFTWARE);// Liso/1.0
    enlist(req_p->cgi_env_list, buf);

    memset(buf, 0, BUF_SIZE);
    strcpy(buf, HTTP_ACCEPT);
    strcat(buf, req_p->http_accept); // ????
    enlist(req_p->cgi_env_list, buf);

    memset(buf, 0, BUF_SIZE);
    strcpy(buf, HTTP_REFERER);
    strcat(buf, req_p->http_referer); // ???
    enlist(req_p->cgi_env_list, buf);

    memset(buf, 0, BUF_SIZE);
    strcpy(buf, HTTP_ACCEPT_ENCODING);
    strcat(buf, req_p->http_accept_encoding);
    enlist(req_p->cgi_env_list, buf);

    memset(buf, 0, BUF_SIZE);
    strcpy(buf, HTTP_ACCEPT_LANGUAGE);
    strcat(buf, req_p->http_accept_language);
    enlist(req_p->cgi_env_list, buf);

    memset(buf, 0, BUF_SIZE);
    strcpy(buf, HTTP_ACCEPT_CHARSET);
    strcat(buf, req_p->http_accept_charset);
    enlist(req_p->cgi_env_list, buf);    

    memset(buf, 0, BUF_SIZE);
    strcpy(buf, HTTP_HOST);
    strcat(buf, req_p->host);
    enlist(req_p->cgi_env_list, buf);    

    memset(buf, 0, BUF_SIZE);
    strcpy(buf, HTTP_COOKIE);
    strcat(buf, req_p->cookie);
    enlist(req_p->cgi_env_list, buf);

    memset(buf, 0, BUF_SIZE);
    strcpy(buf, HTTP_USER_AGENT);
    strcat(buf, req_p->user_agent);
    enlist(req_p->cgi_env_list, buf);        

    memset(buf, 0, BUF_SIZE);
    strcpy(buf, HTTP_CONNECTION);
    strcat(buf, req_p->connection);
    enlist(req_p->cgi_env_list, buf);            

    memset(buf, 0, BUF_SIZE);
    strcpy(buf, HTTPS);
    strcat(buf, bufp->https);
    enlist(req_p->cgi_env_list, buf);            


    return 0;
}



void cgi_FD_CLR(struct buf *bufp, fd_set *master_read_fds, fd_set *master_write_fds){
    struct http_req *req_p;
    
    req_p = bufp->http_reply_p;

    FD_CLR(req_p->fds[0], master_read_fds);
    FD_CLR(req_p->fds[1], master_write_fds);

}


int recv_from_cgi(int fd, struct buf *bufp) {
    int readbytes;
    
    if (bufp->buf_free_size == 0) {
	dbprintf("recv_from_cgi: buf_free_size == 0, wait for send_response to free some space\n");
	return bufp->buf_size; 
    } 
    
    if ((readbytes = read(fd, bufp->buf_tail, bufp->buf_free_size)) == -1) {
	return -1;
    } else if (readbytes == 0) {
	return 0;
    } 

    dbprintf("recv_from_cgi:received %d bytes:%s\n", readbytes, bufp->buf_tail);

    bufp->buf_tail += readbytes;
    bufp->buf_free_size -= readbytes;
    bufp->buf_size += readbytes;

    return bufp->buf_size;
}

int send_to_cgi(int i, struct buf *bufp) {

    struct http_req *p;
    int writebytes;

    p = bufp->http_reply_p;

    if ((writebytes = write(i, p->contp, strlen(p->contp))) != p->cont_len) {
	dbprintf("Error! send_to_cgi, sendbytes:%d, cont_len:%d, actually it's not implemented correctly, fix it\n", writebytes, p->cont_len);
	return -1;
    }

    dbprintf("send_to_cgi: %d bytes written:%s\n", writebytes, p->contp);
    
    bufp->cgi_fully_sent = 1;
    return p->cont_len;
}
