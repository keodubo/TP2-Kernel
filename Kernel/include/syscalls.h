#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include "sched.h"

uint64_t sys_getpid(void);
uint64_t sys_yield(void);
uint64_t sys_exit(int code);
uint64_t sys_nice(int pid, int new_prio);
uint64_t sys_block(int pid);
uint64_t sys_unblock(int pid);
uint64_t sys_proc_snapshot(proc_info_t *buffer, uint64_t max_count);

#endif
