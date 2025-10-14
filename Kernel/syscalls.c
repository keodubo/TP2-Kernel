#include "include/syscalls.h"
#include "include/sched.h"
#include "include/semaphore.h"
#include "include/interrupts.h"

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
