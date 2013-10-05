#ifndef CGI_PARSER_H
#define CGI_PARSER_H

#include "helper.h"

int init_cgi(struct buf *bufp);
int run_cgi(struct buf *bufp, fd_set *master_write_fds, fd_set *master_read_fds);
int locate_cgi(struct buf *bufp);
int parse_cgi_uri(struct buf *bufp);


#endif
