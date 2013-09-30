#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "helper.h"

int recv_request(int sock, struct buf *bufp);
int parse_request(struct buf *bufp);

int parse_request_line(struct buf *bufp);
int parse_request_headers(struct buf *bufp);
int parse_message_body(struct buf *bufp);


#endif
