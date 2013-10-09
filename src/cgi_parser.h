#ifndef CGI_PARSER_H
#define CGI_PARSER_H

#include "helper.h"

int init_cgi(struct buf *bufp);
int run_cgi(struct buf *bufp);
int locate_cgi(struct buf *bufp);
int parse_cgi_uri(struct buf *bufp);

int recv_from_cgi(int fd, struct buf *bufp);
int send_to_cgi(int i, struct buf *bufp);

void cgi_FD_CLR(struct buf *bufp, fd_set *master_read_fds, fd_set *master_write_fds);


#endif
