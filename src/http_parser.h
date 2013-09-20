#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "helper.h"

int parse_req(struct buf *bufp, struct http_req_t *http_req_p);

#endif
