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


int main(int argc, char* argv[])
{
    if (argc < 3) {
	fprintf(stderr, "Usage: ./lisod <HTTP port> <SSL port> ...\n");
	return EXIT_FAILURE;
    }

    int sock, client_sock, ssl_sock;
    int recv_ret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr, ssl_addr;
    struct buf* buf_pts[MAX_SOCK]; // array of pointers to struct buf
    char clientIP[INET6_ADDRSTRLEN];
    fd_set read_fds, write_fds;
    fd_set master_read_fds, master_write_fds;
    int maxfd, i;

    const int ECHO_PORT = atoi(argv[1]);
    const int ssl_port = atoi(argv[2]);
    SSL_CTX *ssl_context;


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
    if (SSL_CTX_use_PrivateKey_file(ssl_context, "../private/ruiqinl.key", SSL_FILETYPE_PEM) == 0) {
	SSL_CTX_free(ssl_context);
	fprintf(stderr, "Error associating private key.\n");
	return EXIT_FAILURE;
    }

    // register public key
    if (SSL_CTX_use_certificate_file(ssl_context, "../certs/ruiqinl.crt", SSL_FILETYPE_PEM) == 0) {
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

			    FD_SET(client_sock, &master_read_fds);
			    buf_pts[client_sock] = (struct buf*) calloc(1, sizeof(struct buf));
			    init_buf(buf_pts[client_sock]); // initialize struct buf
			    if (i == ssl_sock) {
				if (init_ssl_contex(buf_pts[client_sock],ssl_context, client_sock) == -1) {
				    close(client_sock);
				    SSL_CTX_free(ssl_context);
				    clear_buf(buf_pts[client_sock]);
				    fprintf(stderr, "Error! init_ssl_contex\n");
				    // ????how to send msg back????
				}
			    }

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
			    FD_SET(i, &master_write_fds);
			    dbprintf("Server: set %d into master_write_fds\n", i);
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
		    
		} // end i == socket
	    } // end FD_ISSET read_fds
	    
	    /* check fd in write_fds  */
	    if (FD_ISSET(i, &write_fds)) {

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

	    } // end FD_ISSET write_fds
	}// end for i
    }

    /* clear up  */
    close_socket(sock);
    clear_buf_array(buf_pts, maxfd);

    return EXIT_SUCCESS;
}



