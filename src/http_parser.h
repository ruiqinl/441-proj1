#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "helper.h"

int recv_request(int sock, struct buf *bufp);
int parse_request(struct buf *bufp);
int create_response(struct buf *bufp);
int send_response(int sock, struct buf *bufp);

void push_header(struct buf *bufp);
char *date_str();
char *connection_str(struct buf *bufp);
char *cont_len_str(struct buf *bufp);
char *cont_type_str(struct buf *bufp);
int check_path(struct buf *bufp);
void add_size(struct buf *bufp, int len);

#endif
