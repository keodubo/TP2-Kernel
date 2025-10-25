#ifndef FD_H
#define FD_H

#include <stdbool.h>
#include <stdint.h>

struct file;
struct fd_table;

// Tipos básicos de file descriptors que maneja el kernel
typedef enum {
    FD_NONE,
    FD_TTY,
    FD_PIPE
} fd_type_t;

typedef struct file file_t;
typedef struct fd_table fd_table_t;

// Operaciones que cada backend debe implementar
struct fd_ops {
    int (*read)(file_t *file, void *buf, int n);
    int (*write)(file_t *file, const void *buf, int n);
    int (*close)(file_t *file);
};

struct file {
    fd_type_t type;
    const struct fd_ops *ops;
    void *ptr;              // Recurso subyacente (tty, pipe, etc.)
    bool can_read;
    bool can_write;
    bool fg_read_guard;
    int refcount;
};

#define FD_MAX     64
#define FD_STDIN    0
#define FD_STDOUT   1
#define FD_STDERR   2

struct fd_table {
    file_t *entries[FD_MAX];
};

void      fd_init(void);
void      fd_init_std(void);

file_t   *file_create(fd_type_t type, void *ptr, bool rd, bool wr, const struct fd_ops *ops);
void      file_ref(file_t *file);
void      file_release(file_t *file);

fd_table_t *fd_table_create(void);
void        fd_table_destroy(fd_table_t *table);
int         fd_table_clone(fd_table_t *dst, fd_table_t *src);
void        fd_table_attach_std(fd_table_t *table);

file_t    *fd_table_get(fd_table_t *table, int fd);
bool       fd_table_can_read(fd_table_t *table, int fd);
bool       fd_table_can_write(fd_table_t *table, int fd);
int        fd_table_dup2(fd_table_t *table, int oldfd, int newfd);
int        fd_table_close(fd_table_t *table, int fd);
int        fd_table_install(fd_table_t *table, int fd, file_t *file);
int        fd_table_allocate(fd_table_t *table, file_t *file);

#endif // FD_H
