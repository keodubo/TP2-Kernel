#include <stddef.h>
#include "include/fd.h"
#include "include/lib.h"

// Tabla global simple de FDs
// TODO: migrar a per-process cuando se implemente gestión completa de procesos
static kfd_t fd_table[FD_MAX];

void fd_init(void) {
    for (int i = 0; i < FD_MAX; i++) {
        fd_table[i].type = FD_NONE;
        fd_table[i].ops = NULL;
        fd_table[i].ptr = NULL;
        fd_table[i].can_read = false;
        fd_table[i].can_write = false;
        fd_table[i].used = false;
    }
    
    // Los primeros 3 FDs son stdin/stdout/stderr (TTY por defecto)
    // Se configurarán cuando se implemente el driver TTY
    fd_table[FD_STDIN].type = FD_TTY;
    fd_table[FD_STDIN].can_read = true;
    fd_table[FD_STDIN].used = true;
    
    fd_table[FD_STDOUT].type = FD_TTY;
    fd_table[FD_STDOUT].can_write = true;
    fd_table[FD_STDOUT].used = true;
    
    fd_table[FD_STDERR].type = FD_TTY;
    fd_table[FD_STDERR].can_write = true;
    fd_table[FD_STDERR].used = true;
}

int fd_alloc(fd_type_t type, void *ptr, bool rd, bool wr, const struct fd_ops *ops) {
    if (ptr == NULL || ops == NULL) {
        return -1;
    }
    
    // Buscar slot libre (empezar desde 3 para reservar stdin/stdout/stderr)
    for (int i = 3; i < FD_MAX; i++) {
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
