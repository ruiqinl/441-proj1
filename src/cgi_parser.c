#include <sys/stat.h>
#include <sys/select.h>
#include <unistd.h>
#include "cgi_parser.h"
#include "helper.h"

const char CONTENT_LENGTH[] = "CONTENT_LENGTH=";
const char CONTENT_TYPE[] = "CONTENT_TYPE=";
const char GATEWAY_INTERFACE[] = "GATEWAY_INTERFACE=CGI/1.1";
const char QUERY_STRING[] = "QUERY_STRING=";
const char REQUEST_METHOD[] = "REQUEST_METHOD=";
const char SCRIPT_NAME[] = "SCRIPT_NAME=";
const char SERVER_PORT[] = "SERVER_PORT=";
const char SERVER_PROTOCOL[] = "SERVER_PROTOCOL=HTTP/1.1";
const char SERVER_SOFTWARE[] = "SERVER_SOFTWARE=Liso/1.0";
const char PATH_INFO[] = "PATH_INFO=";
const char SERVER_NAME[] = "Liso";

int init_cgi(struct buf *bufp) {

    dbprintf("bufp->http_req_p->cgi_arg_list:\n");
    dbprintf_arglist(bufp->http_req_p->cgi_arg_list);
    dbprintf("bufp->http_req_p->cgi_env_list:\n");
    dbprintf_arglist(bufp->http_req_p->cgi_env_list);

    if (locate_cgi(bufp) == 0) {
	dbprintf("init_cgi: cgi file located\n");
    } else {
	dbprintf("init_cgi: cgi file not located\n");
	return -1;
    }

    
    
    return 0;
}

int run_cgi (struct buf *bufp, fd_set *master_write_fds, fd_set *master_read_fds) {
    
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

    // keep record of fds 
    (req_p->fds)[0] = cgi_outpipe[0];
    (req_p->fds)[1] = cgi_inpipe[1];
    
    if((pid = fork()) == -1) {
	perror("Error! run_cgi, fork");
	return -1;
    }

    if (pid == 0) {
	// child process
	dup2(cgi_outpipe[1], STDOUT_FILENO);
	dup2(cgi_inpipe[0], STDIN_FILENO);
	// close all unused fds
	close(cgi_outpipe[0]);
	close(cgi_outpipe[1]);
	close(cgi_inpipe[0]);
	close(cgi_inpipe[1]);
	
	dbprintf("run_cgi: child process, execve cgi %s\n", bufp->path);
	execve(bufp->path, req_p->cgi_arg_list, req_p->cgi_env_list);
	
	// execve returns, error
	perror("Error! run_cgi");
	return -1;
    } 
    
    //parent process
    close(cgi_outpipe[1]);
    close(cgi_inpipe[0]);

    // keep track of cgi fds
    set_cgifd(cgi_outpipe[0]);
    set_cgifd(cgi_inpipe[1]);
    
    dbprintf("run_cgi: parent process, set %d/%d to master_write/read_fds\n", cgi_inpipe[1], cgi_outpipe[0]);
    FD_SET(cgi_inpipe[1], master_write_fds);
    FD_SET(cgi_outpipe[0], master_read_fds);

    // init buf ???
    //init_buf(bup_[cgi_inpipe[1]]);
    //init_buf(buf_pts[cgi_outpipe[0]]);
 
    return 0;
}

int locate_cgi(struct buf *bufp) {
    struct http_req *req_p;
    char buf[PATH_MAX];
    struct stat status;
    
    req_p = bufp->http_req_p;

    strcpy(buf, ROOT);
    strcat(buf, CGI);
    strcat(buf, bufp->cgiscript);
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

    // bufp->path is set inside lisod.c
    // script name is the only item in arg_list
    if ((p1 = strrchr(bufp->path, '/')) != NULL) {
	memset(buf, 0, BUF_SIZE);
	strcpy(buf, p1);
	enlist(req_p->cgi_arg_list, buf);
    } else 	
	enlist(req_p->cgi_arg_list, bufp->path);
    
    // env_list: path, query string
    if((p1 = strstr(req_p->uri, CGI)) == NULL) {
	fprintf(stderr, "Error! parse_cgi_uri, cannot find sub-string /cgi/");
	return -1;
    }
    p1 += strlen(CGI);
    
    if ((p2 = strchr(p1, '?')) == NULL) {
	dbprintf("parse_cgi_uri: ? not found, query string is empty\n");

	// path
	memset(buf, 0, BUF_SIZE);
	strcpy(buf, "/");
	strcat(buf, PATH_INFO);
	strcat(buf, p1);
	enlist(req_p->cgi_env_list, buf);

	// query string, empty
	memset(buf, 0, BUF_SIZE);
	strcpy(buf, QUERY_STRING);
	enlist(req_p->cgi_env_list, buf);
    } else {
	dbprintf("parse_cgi_uri: ? found, query string is not empty\n");
	
	//path
	memset(buf, 0, BUF_SIZE);
	strcpy(buf, PATH_INFO);
	strncat(buf, p1, p2-p1);
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

    //env_list: remote_addr? must?
    
    //env_list: method
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, REQUEST_METHOD);
    strcat(buf, req_p->method);
    enlist(req_p->cgi_env_list, buf);

    //env_list: script name
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, SCRIPT_NAME);
    strcat(buf, req_p->cgi_arg_list[0]);
    enlist(req_p->cgi_env_list, buf);

    //env_list: server_name
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, SERVER_NAME);//lisod
    enlist(req_p->cgi_env_list, buf);
    
    //env_list: server_port
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, SERVER_PORT);
    sprintf(buf+strlen(SERVER_PORT), "%d", bufp->port); // add to buf later
    enlist(req_p->cgi_env_list, buf);

    //env_list: server_protocol
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, SERVER_PROTOCOL);// HTTP/1.1
    enlist(req_p->cgi_env_list, buf);

    //env_list: serve_software
    memset(buf, 0, BUF_SIZE);
    strcpy(buf, SERVER_SOFTWARE);// Liso/1.0
    enlist(req_p->cgi_env_list, buf);


    return 0;
}



void cgi_FD_CLR(struct buf *bufp, fd_set *master_read_fds, fd_set *master_write_fds){
    struct http_req *req_p;
    
    req_p = bufp->http_reply_p;

    FD_CLR(req_p->fds[0], master_read_fds);
    FD_CLR(req_p->fds[1], master_write_fds);

}
