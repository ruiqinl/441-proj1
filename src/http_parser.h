#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "helper.h"

int recv_request(int sock, struct buf *bufp);
int parse_request(struct buf *bufp);
int create_response(struct buf *bufp);
int send_response(int sock, struct buf *bufp);

#endif
