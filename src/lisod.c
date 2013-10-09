/******************************************************************************
* lisod.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.                               *
*                                                                             *
*                                                                             *
*******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>

#include "helper.h"
#include "http_parser.h"
#include "http_replyer.h"
#include "cgi_parser.h"

int main(int argc, char* argv[])
{
    if (argc < 3) {
	fprintf(stderr, "usage: ./lisod <HTTP port> <HTTPS port> <log file> <lock file> <www folder> <CGI folder or script name> <private key file> <certificate file>\n");
	return EXIT_FAILURE;
    }


    int sock, client_sock, ssl_sock;
    int recv_ret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr, ssl_addr;
    struct buf* buf_pts[MAX_SOCK]; // array of pointers to struct buf
    struct http_req *req_p;
    char clientIP[INET6_ADDRSTRLEN];
    fd_set read_fds, write_fds;
    fd_set master_read_fds, master_write_fds;
    int maxfd, i;
    SSL_CTX *ssl_context;
    
    const int ECHO_PORT = atoi(argv[1]);
    const int ssl_port = atoi(argv[2]);
    const char *logfile = argv[3];
    const char *lockfile = argv[4];
    const char *www = argv[5];
    const char *cgiscript = argv[6];
    //const char *cgiscript = "../flaskr/wsgi_wrapper.py";
    const char *privatekey = argv[7];
    const char *cert = argv[8];

    int j;
    for (j = 0; j < MAX_SOCK; j++) {
	pipe_buf_array[j] = NULL;
    }

    fprintf(stdout, "----- Echo Server -----\n");

    /* http server socket  */

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    } 

  
    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }


    /* ssl init */
    SSL_load_error_strings();
    SSL_library_init();

    // we want to use TLSv1 only
    if ((ssl_context = SSL_CTX_new(TLSv1_server_method())) == NULL) {
	fprintf(stderr, "Error creating SSL contex.\n");
	return EXIT_FAILURE;
    }

    // register private key
    //if (SSL_CTX_use_PrivateKey_file(ssl_context, "../private/ruiqinl.key", SSL_FILETYPE_PEM) == 0) {
    dbprintf("private key:%s\n", privatekey);
    if (SSL_CTX_use_PrivateKey_file(ssl_context, privatekey, SSL_FILETYPE_PEM) == 0) {
	SSL_CTX_free(ssl_context);
	fprintf(stderr, "Error associating private key.\n");
	return EXIT_FAILURE;
    }

    // register public key
    //if (SSL_CTX_use_certificate_file(ssl_context, "../certs/ruiqinl.crt", SSL_FILETYPE_PEM) == 0) {
    if (SSL_CTX_use_certificate_file(ssl_context, cert, SSL_FILETYPE_PEM) == 0) {
	SSL_CTX_free(ssl_context);
	fprintf(stderr, "Error associating certificate.\n");
	return EXIT_FAILURE;
    }
    
    /* ssl http server socket  */

    /* all networked programs must create a socket */
    if ((ssl_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
	SSL_CTX_free(ssl_context);
        fprintf(stderr, "Failed creating ssl socket.\n");
        return EXIT_FAILURE;
    }

    ssl_addr.sin_family = AF_INET;
    ssl_addr.sin_port = htons(ssl_port);
    ssl_addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(ssl_sock, (struct sockaddr *) &ssl_addr, sizeof(ssl_addr)))
    {
        close_socket(ssl_sock);
	SSL_CTX_free(ssl_context);
        fprintf(stderr, "Failed binding ssl_socket.\n");
        return EXIT_FAILURE;
    } 

  
    if (listen(ssl_sock, 5))
    {
        close_socket(ssl_sock);
	SSL_CTX_free(ssl_context);
        fprintf(stderr, "Error listening on ssl_socket.\n");
        return EXIT_FAILURE;
    }


    /* make copies of read_fds and write_fds */
    FD_ZERO(&master_write_fds);
    FD_ZERO(&master_read_fds);
    FD_SET(sock, &master_read_fds);
    FD_SET(ssl_sock, &master_read_fds);

    dbprintf("sock:%d, ssl_sock:%d\n", sock, ssl_sock);
    if (sock > ssl_sock)
	maxfd = sock;
    else 
	maxfd = ssl_sock;

    /* run until coming across errors */
    while (1) {

	/* reset read_fds and write_fds  */
	read_fds = master_read_fds;
	write_fds = master_write_fds;

	if (select(maxfd+1, &read_fds, &write_fds, NULL, NULL) == -1) {
	    perror("Error! select");
	    close_socket(sock);
	    clear_buf_array(buf_pts, maxfd);
	    return EXIT_FAILURE;
	} 
	
	/* check file descriptors in read_fds and write_fds*/
	for (i = 0; i <= maxfd; i++) {

	    /* check fd in read_fds */
	    if (FD_ISSET(i, &read_fds)) {

		if (i == sock || i == ssl_sock) {
		    /* listinging sockte is ready, server receives new connection */

		    if (i == ssl_sock) {
			dbprintf("Server: ssl server received new connection\n");
			client_sock = accept(ssl_sock, (struct sockaddr *)&cli_addr, &cli_size);
		    }
		    else {
			dbprintf("Server: http server received new connection\n");
			client_sock = accept(sock, (struct sockaddr *)&cli_addr, &cli_size);
		    }

		    if (client_sock == -1) {
			perror("Error! accept error! ignore it");
		    } else {

			dbprintf("Server: received new connection from %s, ", inet_ntop(AF_INET, &(cli_addr.sin_addr), clientIP, INET6_ADDRSTRLEN)); 
			dbprintf("socket %d is created\n", client_sock); // debug print

			/* alloc buffer only if the client_sock is smaller than MAX_SOCK  */
			if (!is_2big(client_sock)) {

			    // general
			    FD_SET(client_sock, &master_read_fds);
			    buf_pts[client_sock] = (struct buf*) calloc(1, sizeof(struct buf));
			    init_buf(buf_pts[client_sock], cgiscript, client_sock, www, &cli_addr, i); // initialize struct buf
			    strcpy(buf_pts[client_sock]->https, "0");

			    // ssl
			    if (i == ssl_sock) {
				strcpy(buf_pts[client_sock]->https, "1");

				if (init_ssl_contex(buf_pts[client_sock],ssl_context, client_sock) == -1) {
				    close(client_sock);
				    SSL_CTX_free(ssl_context);
				    clear_buf(buf_pts[client_sock]);
				    fprintf(stderr, "Error! Server, init_ssl_contex\n");
				    // ????how to send msg back????
				}
			    } 

			    // cgi
			    buf_pts[client_sock]->server_port = i; 

			    dbprintf("buf_pts[%d] allocated, rbuf_free_size:%d\n", client_sock, buf_pts[client_sock]->rbuf_free_size);
			    /* track maxfd */
			    if (client_sock > maxfd)
				maxfd = client_sock;

			} else {
			    send_error(client_sock, MSG503); // send MSG503 back to client in non-block way
			    close_socket(client_sock);
			}	    
		    }
		    
		} else {
		    /* conneciton socket is ready, read */
		    /* it could also be a cgi pipe fd */

		    if (pipe_buf_array[i] != NULL && pipe_buf_array[i]->cgi_fully_received == 0) {
			dbprintf("Server: recv data from cgi pipe %d\n", i);
			
			if ((recv_ret = recv_from_cgi(i, pipe_buf_array[i])) == -1) {
			    dbprintf("Error! Server, recv_from_cgi return -1\n");
			    FD_CLR(i, &master_read_fds);
			    // ??? clear up????
			} else if (recv_ret == 0) {
			    dbprintf("Server: recv_from_cgi return 0, fully read, FD_CLR %d from &master_read_fds\n", i);
			    pipe_buf_array[i]->cgi_fully_received = 0; 
			    pipe_buf_array[i] = NULL;// reset
			    FD_CLR(i, &master_read_fds);
			} else if (recv_ret > 0) {
			    dbprintf("Server: recv_from_cgi recvs %d bytes, set buf_sock %d to master_write_fds\n", recv_ret, pipe_buf_array[i]->buf_sock);
			    FD_SET(pipe_buf_array[i]->buf_sock, &master_write_fds);
			    pipe_buf_array[i]->res_fully_created = 1;
			    pipe_buf_array[i]->res_fully_sent = 0;
			}
			
		    } else if (pipe_buf_array[i] != NULL && pipe_buf_array[i]->cgi_fully_received == 1) {
			dbprintf("Server: this line should never be reached!!!! fix it\n");
			FD_CLR(i, &master_read_fds);
		    } else {

			dbprintf("Server: recv request from client sock %d\n", i);

			/* recv_ret -1: recv error; 0: recv 0; 1: recv some bytes */
			recv_ret = recv_request(i, buf_pts[i]); 

			dbprintf("===========================================================\n");
			dbprintf("Server: recv_request from sock %d, recv_ret is %d\n", i, recv_ret);

			if (recv_ret == 1){

			    dbprintf("Server: parse request from sock %d\n", i);
			    parse_request(buf_pts[i]); //set req_count, and push request into req_queue
			    dbprint_queue(buf_pts[i]->req_queue_p);

			    // if there is request in req_queue, set fd in master_write_fds and reply
			    if (buf_pts[i]->req_queue_p->req_count > 0) {
			    
				req_p = req_peek(buf_pts[i]->req_queue_p);
				if (strncmp(req_p->uri, CGI, strlen(CGI)) == 0) {
				    dbprintf("Server: cgi request\n");
				    buf_pts[i]->is_cgi_req = 1; // don't really need this line

				    dequeue_request(buf_pts[i]); // cgi request dequeues here, static request later

				    // not really necessary, remove locate_cgi later??????????
				    if (locate_cgi(buf_pts[i]) == -1) {
					fprintf(stderr, "Error! Server, locate_cgi\n");
					// ???? send error msg to client????
				    }

				    parse_cgi_uri(buf_pts[i]);
				    dbprintf("bufp->http_req_p->cgi_arg_list:\n");
				    dbprintf_arglist(buf_pts[i]->http_req_p->cgi_arg_list);
				    dbprintf("bufp->http_req_p->cgi_env_list:\n");
				    dbprintf_arglist(buf_pts[i]->http_req_p->cgi_env_list);

				    if (run_cgi(buf_pts[i]) == -1 ) {
					dbprintf("Error! Server, init_cgi/run_cgi\n");
					cgi_FD_CLR(buf_pts[i], &master_read_fds, &master_write_fds);
					clear_buf(buf_pts[i]);
					close_socket(i);
				    } else {
				    	dbprintf("Server: parent process, set %d/%d to master_write/read_fds\n", buf_pts[i]->http_reply_p->fds[1], buf_pts[i]->http_reply_p->fds[0]);
					if (buf_pts[i]->http_reply_p->cont_len != 0)
					    FD_SET(buf_pts[i]->http_reply_p->fds[1], &master_write_fds);  
				        FD_SET(buf_pts[i]->http_reply_p->fds[0], &master_read_fds);

					//keep track of max fd
					if (maxfd < buf_pts[i]->http_reply_p->fds[1])
					    maxfd = buf_pts[i]->http_reply_p->fds[1];
					if (maxfd < buf_pts[i]->http_reply_p->fds[0])
					    maxfd = buf_pts[i]->http_reply_p->fds[0];

				    }

				} else {
				    dbprintf("Server: static request\n");
				    FD_SET(i, &master_write_fds);
				    buf_pts[i]->is_cgi_req = 0;
				    dbprintf("Server: set %d into master_write_fds\n", i);
				}
			    
			    
			    }

			} else {

			    if (recv_ret == -1) {
				perror("Error! Server, recv_request, clearup buf");
			    } else if ( recv_ret == 0) { 
				dbprintf("Server: client_sock %d closed, clearup buf\n",i);
			    }

			    /* clear up  */
			    FD_CLR(i, &master_read_fds);
			    FD_CLR(i, &master_write_fds); 
			    clear_buf(buf_pts[i]);
			    close_socket(i);
			}

		    }

		    
		} // end i
	    } // end FD_ISSET read_fds
	    
	    /* check fd in write_fds  */
	    if (FD_ISSET(i, &write_fds)) {

		//dbprintf("pipe_buf_array[i]:%p\n", pipe_buf_array[i]);
		//dbprintf("pipe_buf_array[i]->cgi_fully_sent:%d\n", pipe_buf_array[i]->cgi_fully_sent);
		if (pipe_buf_array[i] != NULL && pipe_buf_array[i]->cgi_fully_sent == 0) {
		    dbprintf("Server: send data to cgi pipe %d\n", i);
		    
		    if (send_to_cgi(i, pipe_buf_array[i]) == -1) {
			FD_CLR(i, &master_write_fds);
		    }

		    if (pipe_buf_array[i]->cgi_fully_sent == 1) {
			dbprintf("Server: send_to_cgi fully sent, FD_CLR %d from &master_write_fds\n", i);
			close(i);
			FD_CLR(i, &master_write_fds);
			pipe_buf_array[i] = NULL; //reset
		    }

		} else if (pipe_buf_array[i] != NULL && pipe_buf_array[i]->cgi_fully_sent == 1) {
		    dbprintf("Server: buf is not empty(recv_from_cgi), send response\n");
		    send_response(i, buf_pts[i]);

		} else {
		    dbprintf("\nServer: create/continue creating response for sock %d\n", i);

		    // have some content in the buffer to send
		    if (create_response(buf_pts[i]) > 0) {

			dbprintf("Server: buf is not empty, send response\n");
			send_response(i, buf_pts[i]);
		    
		    } else {
			dbprintf("Server: no more content to create, stop sending, reset buf\n");
		    
			// clear up
			buf_pts[i]->res_fully_sent = 1;
			reset_buf(buf_pts[i]); // do not free the buf, keep it for next read

			if (buf_pts[i]->req_queue_p->req_count == 0) {
			    FD_CLR(i, &master_write_fds);
			    reset_buf(buf_pts[i]);
			    dbprintf("Server: req_count reaches 0, FD_CLR %d from write_fds\n\n",i);
			}
		    }		    

		}

		
	    } // end FD_ISSET write_fds
	}// end for i
    }

    /* clear up  */
    close_socket(sock);
    clear_buf_array(buf_pts, maxfd);

    return EXIT_SUCCESS;
}



