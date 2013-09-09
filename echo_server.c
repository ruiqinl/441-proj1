/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
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

#define ECHO_PORT 9999
#define BUF_SIZE 4096
#define IP_BUF_SIZE 128
#define MAX_SOCK 1024

#define DEBUG 0
#define dbprintf(...) do{if(DEBUG) fprintf(stderr, __VA_ARGS__); }while(0)

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

struct buf {
    char *buf;
    char *p;
    ssize_t size;

    int allocated;
};

void init_buf(struct buf* bufp){

    bufp->buf = (char *) calloc(BUF_SIZE, sizeof(char));
    bufp->p = bufp->buf;
    bufp->size = 0;

    bufp->allocated = 1;
}

void reset_buf(struct buf* bufp) {
    
    bufp->p = bufp->buf;
    bufp->size = 0;
}

void clear_buf(struct buf* bufp){
    if (bufp->buf != NULL)
	free(bufp->buf);
    bufp->p = NULL;
    bufp->size = 0;
    
    bufp->allocated = 0;
}


int is_2big(int fd) {
    if (fd >= MAX_SOCK) {
	fprintf(stderr, "Warning! fd %d >= MAX_SOCK %d, it's ignored, the client might block\n", fd, MAX_SOCK);
	return 1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    int sock, client_sock;
    ssize_t readret, sendret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    //    char buf[BUF_SIZE];
    struct buf* buf_pts[MAX_SOCK]; // array of pointers to struct buf
    char clientIP[INET6_ADDRSTRLEN];
    fd_set read_fds, write_fds;
    fd_set master_read_fds, master_write_fds;
    int maxfd, i;

    fprintf(stdout, "----- Echo Server -----\n");
    
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

    /* make copies of read_fds and write_fds */
    FD_ZERO(&master_write_fds);
    FD_ZERO(&master_read_fds);
    FD_SET(sock, &master_read_fds);
    
    /* buffer resource is limited, if a socket is larger than MAX_SOCK, it's ignored  */
    if (!is_2big(sock))
	maxfd = sock;
    
    /* run until coming across errors */
    while (1) {

	/* reset read_fds and write_fds  */
	read_fds = master_read_fds;
	write_fds = master_write_fds;
	if (select(maxfd+1, &read_fds, &write_fds, NULL, NULL) == -1) {
	    perror("Error! select");
	    return EXIT_FAILURE;
	} 
	
	/* check file descriptors in read_fds and write_fds*/
	for (i = 0; i <= maxfd; i++) {

	    /* check fd in read_fds */
	    if (FD_ISSET(i, &read_fds)) {

		if (i == sock) {

		    /* server receives new connection */
		    if ((client_sock = accept(sock, (struct sockaddr *)&cli_addr, &cli_size)) == -1){
			close_socket(sock);
			perror("Error! accept");
			return EXIT_FAILURE;
		    }

		    dbprintf("Server: received new connection from %s, ", inet_ntop(AF_INET, &(cli_addr.sin_addr), clientIP, INET6_ADDRSTRLEN)); // debug print
		    dbprintf("socket %d is created\n", client_sock);

		    /* alloc buffer only if the client_sock is smaller than MAX_SOCK  */
		    if (!is_2big(client_sock)){

			FD_SET(client_sock, &master_read_fds);
			buf_pts[client_sock] = (struct buf*) calloc(1, sizeof(struct buf));
			init_buf(buf_pts[client_sock]); // initialize struct buf
			dbprintf("allocated:%d\n", buf_pts[client_sock]->allocated);

			// track maxfd
			if (client_sock > maxfd)
			    maxfd = client_sock;
		    }		    
		    
		} else {

		    /* receive error */
		    if ((readret = recv(i, buf_pts[i]->buf, BUF_SIZE, 0)) <= 0) {
			
			if (readret == -1) {
			    perror("Error! recv");

			    dbprintf("before clear_buf recv, alloc:%d\n", buf_pts[i]->allocated);
			    reset_buf(buf_pts[i]);
			    dbprintf("after clear_buf recv, alloc:%d\n", buf_pts[i]->allocated);

			    return EXIT_FAILURE;
			} else if ( readret == 0) { 
			    dbprintf("Server: client_sock %d closed\n",i);
			} 
			
			/* clear up  */
			close_socket(i);
			FD_CLR(i, &master_read_fds);
			

		    } else {

			/* server receives data from a client
			 * set this socket in master_write_fds 
			 * and send the data back when the next select returns
			 */
			FD_SET(i, &master_write_fds);
			buf_pts[i]->size += readret; // increase size, p does not change
			
		    }
		    
		} // end i == socket
	    } // end FD_ISSET read_fds
	    
	    /* check fd in write_fds  */
	    if (FD_ISSET(i, &write_fds)) {
		
		if ((sendret = send(i, buf_pts[i]->buf, buf_pts[i]->size, 0)) != buf_pts[i]->size) {
		    close_socket(i);
		    perror("Error! send");
		    fprintf(stderr, "sendret=%ld, readret=%ld\n", sendret, buf_pts[i]->size);
		    return EXIT_FAILURE;
		}

		dbprintf("Server: received %ld bytes data, sent %ld bytes back to client_sock %d\n", buf_pts[i]->size, sendret, i); // debug print
		dbprintf("buf reset\n");

		FD_CLR(i, &master_write_fds);
		reset_buf(buf_pts[i]); // reset struct buf

	    } // end FD_ISSET write_fds

	}// end for i

    }

    close_socket(sock);
    
    for (i = 0; i <= maxfd; i++ ) {
	if (buf_pts[i]->allocated == 1) {
	    dbprintf("before clear_buf send, alloc:%d\n", buf_pts[i]->allocated);
	    clear_buf(buf_pts[i]);
	    dbprintf("before clear_buf send, alloc:%d\n", buf_pts[i]->allocated);
	}
    }
    

    return EXIT_SUCCESS;
}
