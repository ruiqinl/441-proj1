#ifndef _HELPER_H_
#define _HELPER_H_

#define MAX_SOCK 2048
#define BUF_SIZE 4096

struct buf {
    char *buf;
    char *p;
    ssize_t size;

    int allocated;
};

void init_buf(struct buf *bufp);
void clear_buf(struct buf *bufp);
void reset_buf(struct buf *bufp);
int is_2big(int fd);

#endif
