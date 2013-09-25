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
#include "helper.h"
#include "http_parser.h"


#define DEBUG 1


int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

void clear_buf_array(struct buf *buf_pts[], int maxfd);

int main(int argc, char* argv[])
{
    if (argc < 2) {
	fprintf(stderr, "Usage: ./lisod <HTTP port> ...\n");
	return EXIT_FAILURE;
    }

    int sock, client_sock;
    int errcode;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    //    char buf[BUF_SIZE];
    struct buf* buf_pts[MAX_SOCK]; // array of pointers to struct buf
    char clientIP[INET6_ADDRSTRLEN];
    fd_set read_fds, write_fds;
    fd_set master_read_fds, master_write_fds;
    int maxfd, i;
    const int ECHO_PORT = atoi(argv[1]);
    struct http_req_t *http_req_p;
    struct buf temp_buf;

    http_req_p = (struct http_req_t *) calloc(1, sizeof(struct http_req_t));
    

    fprintf(stdout, "----- Echo Server -----\n");
    
    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }
    dbprintf("sock %d\n", sock);

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
    
    maxfd = sock;

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

		if (i == sock) {
		    dbprintf("!!!new connection!!! might be missed!!!\n");
		    /* listinging sockte is ready, server receives new connection */

		    if ((client_sock = accept(sock, (struct sockaddr *)&cli_addr, &cli_size)) == -1){
			perror("Error! accept error! ignore it");
		    } else {

			dbprintf("Server: received new connection from %s, ", inet_ntop(AF_INET, &(cli_addr.sin_addr), clientIP, INET6_ADDRSTRLEN)); 
			dbprintf("socket %d is created\n", client_sock); // debug print

			/* alloc buffer only if the client_sock is smaller than MAX_SOCK  */
			if (!is_2big(client_sock)) {

			    FD_SET(client_sock, &master_read_fds);
			    buf_pts[client_sock] = (struct buf*) calloc(1, sizeof(struct buf));
			    init_buf(buf_pts[client_sock]); // initialize struct buf
			    dbprintf("buf_pts[%d] allocated\n", client_sock);

			    /* track maxfd */
			    if (client_sock > maxfd)
				maxfd = client_sock;

			} else {
			    /* cp1: client_sock is larger than MAX_SOCK, close it 
			     * client might receive error
			     * cp2: return 503 service unavailable response ??? how to send msg back???
			     */
			    init_buf(&temp_buf);
			    temp_buf.code = 6;
			    close_socket(client_sock);
			}	    
		    }
		    
		} else {
		    dbprintf("In FD_ISSET read_fds %d\n", i);
		    /* conneciton socket is ready, read */

		    /* errcode, -1:recv error, 0: recv 0, 1: continue reading, 2: finished reading, 3: error, cannot find Content-Length header(POST), 4: find no method , 5 buffer overflow , 6 service unavailable--set only when accepting too many client, 7 http version not 1.1 msg505 */
		    dbprintf("Server: recv req from sock %d\n", i);
		    errcode = recv_request(i, buf_pts[i]); // receive and parse request

		    buf_pts[i]->code = errcode;
		    dbprintf("Server: errcode:%d\n", buf_pts[i]->code);
		    
		    if (errcode <= 0) {
			if (errcode == -1) {
			    perror("Error! recv error! close this socket and clear its buffer");
			} else if ( errcode == 0) { 
			    // client socket closed
			    dbprintf("Server: client_sock %d closed\n",i);
			}

			/* clear up  */
			

			FD_CLR(i, &master_read_fds);
			
			
			clear_buf(buf_pts[i]);
			close_socket(i);
			
		    } else if (errcode == 1) {
			// do nothing, continue receiving 
			dbprintf("Server: request not fully received\n\n");

		    } else {
			if (errcode == 2)
			    dbprintf("Server: request just/already received\n");
			else if (errcode == 3)
			    fprintf(stderr, "Server: error, POST method, but cannot find Content-Length header\n"); 
			else if (errcode == 4)
			    fprintf(stderr, "Server: error, method not found\n");
			else if (errcode == 5)
			    fprintf(stderr, "Server: error, buffer overflow\n");
			else if (errcode == 7)
			    dbprintf("Server: http version is not 1.1\n");
			
			FD_CLR(i, &master_read_fds); // stop reading 

			FD_SET(i, &master_write_fds); // start creating reply and sending reply
			
			reset_buf(buf_pts[i]); // reset the buffer inside the struct buf
		
			dbprintf("Server: after reset_buf, buf:%s\n", buf_pts[i]->buf);

		    } 
		    
		} // end i == socket
	    } // end FD_ISSET read_fds
	    
	    /* check fd in write_fds  */
	    if (FD_ISSET(i, &write_fds)) {
		dbprintf("\nIn FD_ISSET write_fds %d\n", i);
		dbprintf("Server: create/continue creating response\n");

		// have some content in the buffer to send
		if (create_response(buf_pts[i]) > 0) {
		    
		    dbprintf("Server: buf is not empty, send response\n");
		    send_response(i, buf_pts[i]);

		} else {
		    dbprintf("Server: buf is empty, stop sending, reset buf\n");

		    // clear up
		    FD_CLR(i, &master_write_fds);
		    reset_buf(buf_pts[i]); // do not free the buf, keep it for next read
		}		    
		
	    } // end FD_ISSET write_fds

	}// end for i

	/*   */

    }

    /* clear up  */
    //close_socket(sock);
    //clear_buf_array(buf_pts, maxfd);

    return EXIT_SUCCESS;
}


void clear_buf_array(struct buf *buf_pts[], int maxfd){

    int i;

    for (i = 0; i <= maxfd; i++ ) {
	if (buf_pts[i]->allocated == 1) {
	    dbprintf("before clear_buf send, alloc:%d\n", buf_pts[i]->allocated);
	    clear_buf(buf_pts[i]);
	    dbprintf("before clear_buf send, alloc:%d\n", buf_pts[i]->allocated);
	}
    }

}

