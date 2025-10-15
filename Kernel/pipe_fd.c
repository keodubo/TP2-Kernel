#include <stddef.h>
#include "include/fd.h"
#include "include/pipe.h"

// Adaptadores para exponer los pipes a través de la interfaz de file descriptors

static int fd_pipe_read(int fd, void *buf, int n) {
    kfd_t *f = fd_get(fd);
    if (f == NULL || !f->can_read || f->ptr == NULL) {
        return -1;
    }
    return kpipe_read((kpipe_t *)f->ptr, buf, n);
}

static int fd_pipe_write(int fd, const void *buf, int n) {
    kfd_t *f = fd_get(fd);
    if (f == NULL || !f->can_write || f->ptr == NULL) {
        return -1;
    }
    return kpipe_write((kpipe_t *)f->ptr, buf, n);
}

static int fd_pipe_close(int fd) {
    kfd_t *f = fd_get(fd);
    if (f == NULL || f->ptr == NULL) {
        return -1;
    }
    return kpipe_close((kpipe_t *)f->ptr, f->can_read, f->can_write);
}

const struct fd_ops PIPE_OPS = {
    .read = fd_pipe_read,
    .write = fd_pipe_write,
    .close = fd_pipe_close
};
