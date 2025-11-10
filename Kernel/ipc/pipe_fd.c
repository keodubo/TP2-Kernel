#include <stddef.h>
#include "fd.h"
#include "pipe.h"

// Adaptadores para exponer los pipes a través de la interfaz de file descriptors

static int fd_pipe_read(file_t *file, void *buf, int n) {
    if (file == NULL || !file->can_read || file->ptr == NULL || buf == NULL || n <= 0) {
        return -1;
    }
    return kpipe_read((kpipe_t *)file->ptr, buf, n);
}

static int fd_pipe_write(file_t *file, const void *buf, int n) {
    if (file == NULL || !file->can_write || file->ptr == NULL || buf == NULL || n <= 0) {
        return -1;
    }
    return kpipe_write((kpipe_t *)file->ptr, buf, n);
}

static int fd_pipe_close(file_t *file) {
    if (file == NULL || file->ptr == NULL) {
        return -1;
    }
    return kpipe_close((kpipe_t *)file->ptr, file->can_read, file->can_write);
}

const struct fd_ops PIPE_OPS = {
    .read = fd_pipe_read,
    .write = fd_pipe_write,
    .close = fd_pipe_close
};

