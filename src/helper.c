
#include <stdio.h>
#include <stdlib.h>
#include "helper.h"

void init_buf(struct buf* bufp){

    bufp->buf = (char *) calloc(BUF_SIZE, sizeof(char));
    bufp->p = bufp->buf; // p is not used yet in checkpoint-1
    bufp->size = 0;

    bufp->allocated = 1;
}

void reset_buf(struct buf* bufp) {
    if (bufp->allocated == 1) {
	bufp->p = bufp->buf;
	bufp->size = 0;
    } else 
	fprintf(stderr, "Warning: reset_buf, bufp is not allocated yet\n");
    
}

void clear_buf(struct buf* bufp){

    if (bufp->allocated == 1)
	free(bufp->buf);
    bufp->p = NULL;
    bufp->size = 0;
    
    bufp->allocated = 0;
}


int is_2big(int fd) {
    if (fd >= MAX_SOCK) {
	fprintf(stderr, "Warning! fd %d >= MAX_SOCK %d, it's closed, the client might receive connection reset error\n", fd, MAX_SOCK);
	return 1;
    }
    return 0;
}
