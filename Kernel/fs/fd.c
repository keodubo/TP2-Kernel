#include <stddef.h>
#include "fd.h"
#include "lib.h"
#include "tty.h"
#include "pipe.h"
#include "interrupts.h"
#include "errno.h"
#include "sched.h"
#include "memory_manager.h"


static file_t *stdin_file = NULL;
static file_t *stdout_file = NULL;
static file_t *stderr_file = NULL;

// Helpers mínimos para ejecutar secciones críticas breves
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

// Adaptadores entre files y backend de TTY
static int fd_tty_read(file_t *file, void *buf, int n) {
    if (file == NULL || !file->can_read || file->ptr == NULL || buf == NULL || n <= 0) {
        return -1;
    }

    pcb_t *cur = sched_current();
    if (cur == NULL) {
        return -1;
    }

    if (file->fg_read_guard && !tty_can_read(cur->pid)) {
        return E_BG_INPUT;
    }

    return tty_read((tty_t *)file->ptr, buf, n);
}

static int fd_tty_write(file_t *file, const void *buf, int n) {
    if (file == NULL || !file->can_write || file->ptr == NULL || buf == NULL || n <= 0) {
        return -1;
    }
    return tty_write((tty_t *)file->ptr, buf, n);
}

static int fd_tty_close(file_t *file) {
    if (file == NULL || file->ptr == NULL) {
        return -1;
    }
    return tty_close((tty_t *)file->ptr);
}

static const struct fd_ops TTY_OPS = {
    .read = fd_tty_read,
    .write = fd_tty_write,
    .close = fd_tty_close
};

void fd_init(void) {
    stdin_file = NULL;
    stdout_file = NULL;
    stderr_file = NULL;
}

void fd_init_std(void) {
    tty_t *tty = tty_default();
    if (tty == NULL) {
        return;
    }

    if (stdin_file == NULL) {
        stdin_file = file_create(FD_TTY, tty, true, false, &TTY_OPS);
        if (stdin_file != NULL) {
            stdin_file->fg_read_guard = true;
        }
    }
    if (stdout_file == NULL) {
        stdout_file = file_create(FD_TTY, tty, false, true, &TTY_OPS);
    }
    if (stderr_file == NULL) {
        stderr_file = file_create(FD_TTY, tty, false, true, &TTY_OPS);
    }
}

file_t *file_create(fd_type_t type, void *ptr, bool rd, bool wr, const struct fd_ops *ops) {
    if (ptr == NULL || ops == NULL) {
        return NULL;
    }

    file_t *file = (file_t *)mm_malloc(sizeof(file_t));
    if (file == NULL) {
        return NULL;
    }

    file->type = type;
    file->ops = ops;
    file->ptr = ptr;
    file->can_read = rd;
    file->can_write = wr;
    file->fg_read_guard = false;
    file->refcount = 1;
    return file;
}

void file_ref(file_t *file) {
    if (file == NULL) {
        return;
    }
    uint64_t flags = irq_save_local();
    file->refcount++;
    irq_restore_local(flags);
}

void file_release(file_t *file) {
    if (file == NULL) {
        return;
    }

    int new_ref;
    uint64_t flags = irq_save_local();
    file->refcount--;
    new_ref = file->refcount;
    irq_restore_local(flags);

    if (new_ref > 0) {
        return;
    }

    if (file->ops != NULL && file->ops->close != NULL) {
        file->ops->close(file);
    }
    mm_free(file);
}

fd_table_t *fd_table_create(void) {
    fd_table_t *table = (fd_table_t *)mm_malloc(sizeof(fd_table_t));
    if (table == NULL) {
        return NULL;
    }
    for (int i = 0; i < FD_MAX; i++) {
        table->entries[i] = NULL;
    }
    return table;
}

void fd_table_destroy(fd_table_t *table) {
    if (table == NULL) {
        return;
    }
    for (int i = 0; i < FD_MAX; i++) {
        if (table->entries[i] != NULL) {
            file_release(table->entries[i]);
            table->entries[i] = NULL;
        }
    }
    mm_free(table);
}

int fd_table_clone(fd_table_t *dst, fd_table_t *src) {
    if (dst == NULL) {
        return -1;
    }

    for (int i = 0; i < FD_MAX; i++) {
        dst->entries[i] = NULL;
    }

    if (src == NULL) {
        fd_table_attach_std(dst);
        return 0;
    }

    for (int i = 0; i < FD_MAX; i++) {
        file_t *file = src->entries[i];
        if (file != NULL) {
            file_ref(file);
            dst->entries[i] = file;
        }
    }
    return 0;
}

void fd_table_attach_std(fd_table_t *table) {
    if (table == NULL) {
        return;
    }

    if (stdin_file != NULL) {
        file_ref(stdin_file);
        table->entries[FD_STDIN] = stdin_file;
    }
    if (stdout_file != NULL) {
        file_ref(stdout_file);
        table->entries[FD_STDOUT] = stdout_file;
    }
    if (stderr_file != NULL) {
        file_ref(stderr_file);
        table->entries[FD_STDERR] = stderr_file;
    }
}

file_t *fd_table_get(fd_table_t *table, int fd) {
    if (table == NULL || fd < 0 || fd >= FD_MAX) {
        return NULL;
    }
    return table->entries[fd];
}

bool fd_table_can_read(fd_table_t *table, int fd) {
    file_t *file = fd_table_get(table, fd);
    return (file != NULL && file->can_read);
}

bool fd_table_can_write(fd_table_t *table, int fd) {
    file_t *file = fd_table_get(table, fd);
    return (file != NULL && file->can_write);
}

int fd_table_close(fd_table_t *table, int fd) {
    if (table == NULL || fd < 0 || fd >= FD_MAX) {
        return -1;
    }
    file_t *file = table->entries[fd];
    if (file == NULL) {
        return -1;
    }
    table->entries[fd] = NULL;
    file_release(file);
    return 0;
}

int fd_table_install(fd_table_t *table, int fd, file_t *file) {
    if (table == NULL || file == NULL || fd < 0 || fd >= FD_MAX) {
        return -1;
    }
    if (table->entries[fd] != NULL) {
        file_release(table->entries[fd]);
    }
    table->entries[fd] = file;
    return fd;
}

int fd_table_allocate(fd_table_t *table, file_t *file) {
    if (table == NULL || file == NULL) {
        return -1;
    }
    for (int i = 0; i < FD_MAX; i++) {
        if (table->entries[i] == NULL) {
            table->entries[i] = file;
            return i;
        }
    }
    return -1;
}

int fd_table_dup2(fd_table_t *table, int oldfd, int newfd) {
    if (table == NULL || oldfd < 0 || oldfd >= FD_MAX || newfd < 0 || newfd >= FD_MAX) {
        return -1;
    }

    file_t *src = table->entries[oldfd];
    if (src == NULL) {
        return -1;
    }

    if (oldfd == newfd) {
        return newfd;
    }

    if (table->entries[newfd] != NULL) {
        file_release(table->entries[newfd]);
    }

    file_ref(src);
    table->entries[newfd] = src;
    return newfd;
}
