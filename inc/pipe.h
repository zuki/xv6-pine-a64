#ifndef INC_PIPE_H
#define INC_PIPE_H

#include <types.h>
#include <linux/fcntl.h>
#include <spinlock.h>
#include <file.h>

#define PIPESIZE 512

#define PIPE2_FLAGS (O_CLOEXEC | O_DIRECT | O_NONBLOCK)

struct pipe {
    struct spinlock lock;
    char data[PIPESIZE];
    uint32_t nread;         // number of bytes read
    uint32_t nwrite;        // number of bytes written
    int readopen;           // read fd is still open
    int writeopen;          // write fd is still open
};

int pipealloc(struct file **f0, struct file **f1);
void pipeclose(struct pipe *pi, int writable);
int pipewrite(struct pipe *pi, uint64_t addr, int n);
int piperead(struct pipe *pi, uint64_t addr, int n);

#endif
