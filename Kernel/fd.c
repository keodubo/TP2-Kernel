#include <stddef.h>
#include "include/fd.h"
#include "include/lib.h"
#include "include/tty.h"
#include "include/pipe.h"
#include "include/interrupts.h"

// Tabla global simple de FDs
// TODO: migrar a per-process cuando se implemente gestión completa de procesos
static kfd_t fd_table[FD_MAX];

static uint64_t irq_save_local(void) {
    uint64_t flags;
    __asm__ volatile("pushfq\n\tpop %0" : "=r"(flags));
    _cli();
    return flags;
}

static void irq_restore_local(uint64_t flags) {
    if (flags & (1ULL << 9)) {
        _sti();
    }
}

static int fd_tty_read(int fd, void *buf, int n) {
    kfd_t *f = fd_get(fd);
    if (f == NULL || !f->can_read || f->ptr == NULL) {
        return -1;
    }
    return tty_read((tty_t *)f->ptr, buf, n);
}

static int fd_tty_write(int fd, const void *buf, int n) {
    kfd_t *f = fd_get(fd);
    if (f == NULL || !f->can_write || f->ptr == NULL) {
        return -1;
    }
    return tty_write((tty_t *)f->ptr, buf, n);
}

static int fd_tty_close(int fd) {
    kfd_t *f = fd_get(fd);
    if (f == NULL || f->ptr == NULL) {
        return -1;
    }
    return tty_close((tty_t *)f->ptr);
}

static const struct fd_ops TTY_OPS = {
    .read = fd_tty_read,
    .write = fd_tty_write,
    .close = fd_tty_close
};

void fd_init(void) {
    for (int i = 0; i < FD_MAX; i++) {
        fd_table[i].type = FD_NONE;
        fd_table[i].ops = NULL;
        fd_table[i].ptr = NULL;
        fd_table[i].can_read = false;
        fd_table[i].can_write = false;
        fd_table[i].used = false;
    }
}

void fd_init_std(void) {
    tty_t *tty = tty_default();
    if (tty == NULL) {
        return;
    }

    if (!fd_table[FD_STDIN].used) {
        fd_alloc(FD_TTY, tty, true, false, &TTY_OPS);
    }
    if (!fd_table[FD_STDOUT].used) {
        fd_alloc(FD_TTY, tty, false, true, &TTY_OPS);
    }
    if (!fd_table[FD_STDERR].used) {
        fd_alloc(FD_TTY, tty, false, true, &TTY_OPS);
    }
}

int fd_alloc(fd_type_t type, void *ptr, bool rd, bool wr, const struct fd_ops *ops) {
    if (ptr == NULL || ops == NULL) {
        return -1;
    }
    
    for (int i = 0; i < FD_MAX; i++) {
        if (!fd_table[i].used) {
            fd_table[i].type = type;
            fd_table[i].ops = ops;
            fd_table[i].ptr = ptr;
            fd_table[i].can_read = rd;
            fd_table[i].can_write = wr;
            fd_table[i].used = true;
            return i;
        }
    }
    
    return -1; // No hay slots disponibles
}

kfd_t* fd_get(int fd) {
    if (fd < 0 || fd >= FD_MAX) {
        return NULL;
    }
    
    if (!fd_table[fd].used) {
        return NULL;
    }
    
    return &fd_table[fd];
}

int fd_close(int fd) {
    if (fd < 0 || fd >= FD_MAX) {
        return -1;
    }
    
    if (!fd_table[fd].used) {
        return -1;
    }
    
    // Llamar al close del ops si existe
    int result = 0;
    if (fd_table[fd].ops != NULL && fd_table[fd].ops->close != NULL) {
        result = fd_table[fd].ops->close(fd);
    }
    
    // Marcar slot como libre
    fd_table[fd].type = FD_NONE;
    fd_table[fd].ops = NULL;
    fd_table[fd].ptr = NULL;
    fd_table[fd].can_read = false;
    fd_table[fd].can_write = false;
    fd_table[fd].used = false;
    
    return result;
}

int fd_dup2(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= FD_MAX || newfd < 0 || newfd >= FD_MAX) {
        return -1;
    }

    if (oldfd == newfd) {
        return newfd;
    }

    kfd_t *src = fd_get(oldfd);
    if (src == NULL) {
        return -1;
    }

    if (fd_table[newfd].used) {
        fd_close(newfd);
    }

    fd_table[newfd].type = src->type;
    fd_table[newfd].ops = src->ops;
    fd_table[newfd].ptr = src->ptr;
    fd_table[newfd].can_read = src->can_read;
    fd_table[newfd].can_write = src->can_write;
    fd_table[newfd].used = true;

    if (src->type == FD_PIPE && src->ptr != NULL) {
        uint64_t flags = irq_save_local();
        kpipe_t *p = (kpipe_t *)src->ptr;
        if (src->can_read) {
            p->readers++;
        }
        if (src->can_write) {
            p->writers++;
        }
        irq_restore_local(flags);
    }

    return newfd;
}
