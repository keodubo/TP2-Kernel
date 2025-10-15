#include "include/syscalls.h"
#include "include/sched.h"
#include "include/semaphore.h"
#include "include/interrupts.h"
#include "include/pipe.h"
#include "include/fd.h"

// Forward declaration del PIPE_OPS definido en pipe_fd.c
extern const struct fd_ops PIPE_OPS;

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

uint64_t sys_proc_snapshot(proc_info_t *buffer, uint64_t max_count) {
    if (buffer == NULL || max_count == 0) {
        return 0;
    }

    if (max_count > (uint64_t)MAX_PROCS) {
        max_count = MAX_PROCS;
    }

    return (uint64_t)proc_snapshot(buffer, (int)max_count);
}

static ksem_t *sem_handles[KSEM_HANDLE_MAX] = {0};

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

static int sem_handle_allocate(ksem_t *sem) {
    uint64_t flags = irq_save_local();
    for (int i = 0; i < KSEM_HANDLE_MAX; i++) {
        if (sem_handles[i] == NULL) {
            sem_handles[i] = sem;
            irq_restore_local(flags);
            return i + 1;
        }
    }
    irq_restore_local(flags);
    return -1;
}

static ksem_t *sem_handle_peek(int handle) {
    if (handle <= 0 || handle > KSEM_HANDLE_MAX) {
        return NULL;
    }
    uint64_t flags = irq_save_local();
    ksem_t *sem = sem_handles[handle - 1];
    irq_restore_local(flags);
    return sem;
}

static ksem_t *sem_handle_detach(int handle) {
    if (handle <= 0 || handle > KSEM_HANDLE_MAX) {
        return NULL;
    }
    uint64_t flags = irq_save_local();
    ksem_t *sem = sem_handles[handle - 1];
    if (sem != NULL) {
        sem_handles[handle - 1] = NULL;
    }
    irq_restore_local(flags);
    return sem;
}

int sys_sem_open(const char *name, unsigned int init) {
    ksem_t *sem = NULL;
    if (ksem_open(name, init, &sem) != 0) {
        return -1;
    }

    int handle = sem_handle_allocate(sem);
    if (handle < 0) {
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

int sys_pipe_open(const char *name, int flags) {
    if (name == NULL) {
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
    
    return fd_alloc(FD_PIPE, p, for_read, for_write, &PIPE_OPS);
}

int sys_pipe_close(int fd) {
    return fd_close(fd);
}

int sys_pipe_read(int fd, void *buf, int n) {
    kfd_t *f = fd_get(fd);
    if (f == NULL || !f->can_read || f->type != FD_PIPE) {
        return -1;
    }
    return kpipe_read((kpipe_t *)f->ptr, buf, n);
}

int sys_pipe_write(int fd, const void *buf, int n) {
    kfd_t *f = fd_get(fd);
    if (f == NULL || !f->can_write || f->type != FD_PIPE) {
        return -1;
    }
    return kpipe_write((kpipe_t *)f->ptr, buf, n);
}

int sys_pipe_unlink(const char *name) {
    return kpipe_unlink(name);
}

// ========================================
// FD genéricos
// ========================================

int sys_read(int fd, void *buf, int n) {
    kfd_t *f = fd_get(fd);
    if (f == NULL || f->ops == NULL || f->ops->read == NULL) {
        return -1;
    }
    return f->ops->read(fd, buf, n);
}

int sys_write(int fd, const void *buf, int n) {
    kfd_t *f = fd_get(fd);
    if (f == NULL || f->ops == NULL || f->ops->write == NULL) {
        return -1;
    }
    return f->ops->write(fd, buf, n);
}

int sys_close(int fd) {
    return fd_close(fd);
}

int sys_dup2(int oldfd, int newfd) {
    return fd_dup2(oldfd, newfd);
}
