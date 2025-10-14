#include "include/syscalls.h"
#include "include/sched.h"

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
