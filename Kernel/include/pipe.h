#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>
#include <stdbool.h>
#include "sched.h"

#define PIPE_CAP 4096
#define PIPE_NAME_MAX 32
#define PIPE_HASH_BUCKETS 16

typedef struct pipe_waiter {
    pcb_t *proc;
    struct pipe_waiter *next;
} pipe_waiter_t;

typedef struct kpipe {
    char name[PIPE_NAME_MAX];
    uint8_t buf[PIPE_CAP];
    uint32_t r, w, size;        // ring buffer: read pos, write pos, current size
    int readers, writers;        // contadores de FDs abiertos
    bool unlinked;               // true si ya se hizo unlink
    pipe_waiter_t *r_head, *r_tail;  // cola FIFO de lectores bloqueados
    pipe_waiter_t *w_head, *w_tail;  // cola FIFO de escritores bloqueados
    struct kpipe *next_hash;     // siguiente en la tabla hash
} kpipe_t;

// APIs públicas
int kpipe_open(const char* name, bool for_read, bool for_write, kpipe_t **out);
int kpipe_close(kpipe_t *p, bool was_read, bool was_write);
int kpipe_read(kpipe_t *p, void *buf, int n);        // BLOQUEANTE
int kpipe_write(kpipe_t *p, const void *buf, int n); // BLOQUEANTE
int kpipe_unlink(const char* name);

#endif // PIPE_H
