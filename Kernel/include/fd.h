#ifndef FD_H
#define FD_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FD_NONE,
    FD_TTY,
    FD_PIPE
} fd_type_t;

struct fd_ops {
    int (*read)(int fd, void *buf, int n);
    int (*write)(int fd, const void *buf, int n);
    int (*close)(int fd);
};

typedef struct kfd {
    fd_type_t type;
    const struct fd_ops *ops;
    void *ptr;              // puntero al recurso (kpipe_t*, etc.)
    bool can_read;
    bool can_write;
    bool used;              // slot ocupado
} kfd_t;

#define FD_MAX 64
#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2

// APIs públicas
void fd_init(void);
int fd_alloc(fd_type_t type, void *ptr, bool rd, bool wr, const struct fd_ops *ops);
kfd_t* fd_get(int fd);
int fd_close(int fd);

#endif // FD_H
