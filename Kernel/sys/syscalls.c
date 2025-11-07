#include "syscalls.h"
#include "sched.h"
#include "semaphore.h"
#include "interrupts.h"
#include "pipe.h"
#include "fd.h"
#include "memory_manager.h"
#include "lib.h"

#ifndef EINVAL
#define EINVAL 22
#endif

// Implementación de cada syscall expuesta a userland

// Forward declaration del PIPE_OPS definido en pipe_fd.c
extern const struct fd_ops PIPE_OPS;
// Archivo: syscalls.c
// Propósito: Implementación de llamadas al sistema (syscalls)
// Resumen: Funciones que implementan la interfaz de syscalls disponibles
//          para userland (getpid, yield, exit, blocking, etc.).

uint64_t sys_getpid(void) {
    pcb_t *cur = sched_current();
    if (cur == NULL) {
        return (uint64_t)-1;
    }
    return (uint64_t)cur->pid;
}

uint64_t sys_yield(void) {
    sched_force_yield();
    return 0;
}

uint64_t sys_exit(int code) {
    proc_exit(code);
    return 0;
}

uint64_t sys_nice(int pid, int new_prio) {
    proc_nice(pid, new_prio);
    return 0;
}

uint64_t sys_block(int pid) {
    return (uint64_t)proc_block(pid);
}

uint64_t sys_unblock(int pid) {
    return (uint64_t)proc_unblock(pid);
}

uint64_t sys_wait_pid(int pid, int *status) {
    if (pid == 0) {
        pid = -1;
    }
    int exit_status = 0;
    int waited_pid = proc_wait(pid, &exit_status);
    if (waited_pid >= 0 && status != NULL) {
        *status = exit_status;
    }
    return (uint64_t)waited_pid;
}

uint64_t sys_wait_children(int *status) {
    // wait_children espera a cualquier proceso hijo (equivalente a wait_pid(-1, status))
    int exit_status = 0;
    int waited_pid = proc_wait(-1, &exit_status);
    if (waited_pid >= 0 && status != NULL) {
        *status = exit_status;
    }
    return (uint64_t)waited_pid;
}

uint64_t sys_proc_snapshot(proc_info_t *buffer, uint64_t max_count) {
    if (buffer == NULL || max_count == 0) {
        return 0;
    }

    if (max_count > (uint64_t)MAX_PROCS) {
        max_count = MAX_PROCS;
    }

    return (uint64_t)proc_snapshot(buffer, (int)max_count);
}

int sys_mm_get_stats(mm_stats_t *user_stats) {
    if (user_stats == NULL) {
        return -EINVAL;
    }

    mm_stats_t kstats;
    mm_collect_stats(&kstats);

    memcpy(user_stats, &kstats, sizeof(kstats));
    return 0;
}

// Handle con ownership para limpieza automática en proc_exit
typedef struct sem_handle {
    ksem_t *sem;
    int owner_pid;
} sem_handle_t;

static sem_handle_t sem_handles[KSEM_HANDLE_MAX] = {0}; // Tabla de handles estilo POSIX

// Helpers locales para proteger la tabla de handles
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

// Reserva un slot de handle para un semáforo abierto
static int sem_handle_allocate(ksem_t *sem, int owner_pid) {
    uint64_t flags = irq_save_local();
    int allocated_count = 0;
    for (int i = 0; i < KSEM_HANDLE_MAX; i++) {
        if (sem_handles[i].sem != NULL) {
            allocated_count++;
        }
        if (sem_handles[i].sem == NULL) {
            sem_handles[i].sem = sem;
            sem_handles[i].owner_pid = owner_pid;
            irq_restore_local(flags);
            return i + 1;
        }
    }
    // DEBUG: Table is full - log it
    irq_restore_local(flags);
    extern void ncPrintDec(uint64_t value);
    extern void ncPrint(const char *string);
    ncPrint("[KERNEL] Handle table FULL! ");
    ncPrintDec(allocated_count);
    ncPrint(" handles allocated\n");
    return -1;
}

// Obtiene el puntero asociado sin modificar la tabla
static ksem_t *sem_handle_peek(int handle) {
    if (handle <= 0 || handle > KSEM_HANDLE_MAX) {
        return NULL;
    }
    uint64_t flags = irq_save_local();
    ksem_t *sem = sem_handles[handle - 1].sem;
    irq_restore_local(flags);
    return sem;
}

// Desasocia el handle en la tabla (ksem_close se encarga del refcount)
static ksem_t *sem_handle_detach(int handle) {
    if (handle <= 0 || handle > KSEM_HANDLE_MAX) {
        return NULL;
    }
    uint64_t flags = irq_save_local();
    ksem_t *sem = sem_handles[handle - 1].sem;
    if (sem != NULL) {
        sem_handles[handle - 1].sem = NULL;
        sem_handles[handle - 1].owner_pid = 0;
    }
    irq_restore_local(flags);
    return sem;
}

// Limpia todos los handles de semáforos pertenecientes a un proceso
// Llamado desde proc_exit para evitar leaks
void sem_cleanup_process_handles(int pid) {
    extern void ncPrintDec(uint64_t value);
    extern void ncPrint(const char *string);
    int cleaned = 0;

    uint64_t flags = irq_save_local();
    for (int i = 0; i < KSEM_HANDLE_MAX; i++) {
        if (sem_handles[i].sem != NULL && sem_handles[i].owner_pid == pid) {
            ksem_t *sem = sem_handles[i].sem;
            sem_handles[i].sem = NULL;
            sem_handles[i].owner_pid = 0;
            cleaned++;
            // Liberar el lock antes de cerrar (ksem_close puede bloquear/yield)
            irq_restore_local(flags);
            ksem_close(sem);
            flags = irq_save_local();
        }
    }
    irq_restore_local(flags);

    if (cleaned > 0) {
        ncPrint("[KERNEL] Cleaned ");
        ncPrintDec(cleaned);
        ncPrint(" handles for PID ");
        ncPrintDec(pid);
        ncPrint("\n");
    }
}

int sys_sem_open(const char *name, unsigned int init) {
    pcb_t *cur = sched_current();
    if (cur == NULL) {
        return -1;
    }

    ksem_t *sem = NULL;
    if (ksem_open(name, init, &sem) != 0) {
        return -1;
    }

    int handle = sem_handle_allocate(sem, cur->pid);
    if (handle < 0) {
        // DEBUG: Handle table full
        extern void ncPrintDec(uint64_t value);
        extern void ncNewline(void);
        extern void ncPrint(const char *string);
        ncPrint("[KERNEL] sem_open failed for PID ");
        ncPrintDec(cur->pid);
        ncPrint(" - handle table full\n");
        ksem_close(sem);
        return -1;
    }
    return handle;
}

int sys_sem_wait(int sem_id) {
    ksem_t *sem = sem_handle_peek(sem_id);
    if (sem == NULL) {
        return -1;
    }
    return ksem_wait(sem);
}

int sys_sem_post(int sem_id) {
    ksem_t *sem = sem_handle_peek(sem_id);
    if (sem == NULL) {
        return -1;
    }
    return ksem_post(sem);
}

int sys_sem_close(int sem_id) {
    ksem_t *sem = sem_handle_detach(sem_id);
    if (sem == NULL) {
        return -1;
    }
    return ksem_close(sem);
}

int sys_sem_unlink(const char *name) {
    return ksem_unlink(name);
}

// ========================================
// Pipes (Hito 5)
// ========================================
// Las funciones delegan en kpipe_* y usan la tabla de FDs para compartir

int sys_pipe_open(const char *name, int flags) {
    if (name == NULL) {
        return -1;
    }

    pcb_t *cur = sched_current();
    if (cur == NULL || cur->fd_table == NULL) {
        return -1;
    }
    
    bool for_read = (flags & 1) != 0;
    bool for_write = (flags & 2) != 0;
    
    if (!for_read && !for_write) {
        return -1; // Debe ser al menos R o W
    }
    
    kpipe_t *p = NULL;
    if (kpipe_open(name, for_read, for_write, &p) < 0) {
        return -1;
    }

    file_t *file = file_create(FD_PIPE, p, for_read, for_write, &PIPE_OPS);
    if (file == NULL) {
        kpipe_close(p, for_read, for_write);
        return -1;
    }

    int fd = fd_table_allocate(cur->fd_table, file);
    if (fd < 0) {
        file_release(file);
    }
    return fd;
}

int sys_pipe_close(int fd) {
    pcb_t *cur = sched_current();
    if (cur == NULL || cur->fd_table == NULL) {
        return -1;
    }
    return fd_table_close(cur->fd_table, fd);
}

int sys_pipe_read(int fd, void *buf, int n) {
    pcb_t *cur = sched_current();
    if (cur == NULL || cur->fd_table == NULL) {
        return -1;
    }
    file_t *file = fd_table_get(cur->fd_table, fd);
    if (file == NULL || file->type != FD_PIPE || !file->can_read || file->ops == NULL || file->ops->read == NULL) {
        return -1;
    }
    return file->ops->read(file, buf, n);
}

int sys_pipe_write(int fd, const void *buf, int n) {
    pcb_t *cur = sched_current();
    if (cur == NULL || cur->fd_table == NULL) {
        return -1;
    }
    file_t *file = fd_table_get(cur->fd_table, fd);
    if (file == NULL || file->type != FD_PIPE || !file->can_write || file->ops == NULL || file->ops->write == NULL) {
        return -1;
    }
    return file->ops->write(file, buf, n);
}

int sys_pipe_unlink(const char *name) {
    return kpipe_unlink(name);
}

// ========================================
// FD genéricos
// ========================================
// Estas wrappers invocan a la vtable del descriptor solicitado
int sys_read(int fd, void *buf, int n) {
    pcb_t *cur = sched_current();
    if (cur == NULL || cur->fd_table == NULL) {
        return -1;
    }
    file_t *file = fd_table_get(cur->fd_table, fd);
    if (file == NULL || file->ops == NULL || file->ops->read == NULL) {
        return -1;
    }
    return file->ops->read(file, buf, n);
}

int sys_write(int fd, const void *buf, int n) {
    pcb_t *cur = sched_current();
    if (cur == NULL || cur->fd_table == NULL) {
        return -1;
    }
    file_t *file = fd_table_get(cur->fd_table, fd);
    if (file == NULL || file->ops == NULL || file->ops->write == NULL) {
        return -1;
    }
    return file->ops->write(file, buf, n);
}

int sys_close(int fd) {
    pcb_t *cur = sched_current();
    if (cur == NULL || cur->fd_table == NULL) {
        return -1;
    }
    return fd_table_close(cur->fd_table, fd);
}

int sys_dup2(int oldfd, int newfd) {
    pcb_t *cur = sched_current();
    if (cur == NULL || cur->fd_table == NULL) {
        return -1;
    }
    return fd_table_dup2(cur->fd_table, oldfd, newfd);
}
