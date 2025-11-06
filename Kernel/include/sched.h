#ifndef SCHED_H
#define SCHED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct wait_result;
struct fd_table;

typedef enum {
    NEW,
    READY,
    RUNNING,
    BLOCKED,
    EXITED
} proc_state_t;

#define MAX_PROCS         128
#define MAX_PRIOS         4
#define MIN_PRIO          0
#define HIGHEST_PRIO      (MAX_PRIOS - 1)
#define DEFAULT_PRIO      2
#define TIME_SLICE_TICKS  5

typedef struct regs_t {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} regs_t;

typedef struct pcb_t {
    int pid;
    char name[32];
    int priority;
    int base_priority;
    proc_state_t state;
    regs_t *kframe;
    uint8_t *kstack_base;
    int parent_pid;
    bool fg;
    int ticks_left;
    struct pcb_t *next;
    /* Campos adicionales para administración interna */
    struct pcb_t *prev;
    struct pcb_t *queue_next;
    struct pcb_t *cleanup_next;
    void (*entry)(int, char **);
    int argc;
    char **argv;
    int exit_code;
    bool used;
    int waiting_for;
    struct pcb_t *child_head;
    struct pcb_t *sibling_next;
    struct pcb_t *waiter_head;
    struct wait_result *wait_res_head;
    struct wait_result *wait_res_tail;
    int pending_exit_pid;
   int pending_exit_code;
    bool pending_exit_valid;
    bool exited;
    bool zombie_reapable;
    struct fd_table *fd_table;
    int aging_ticks;
} pcb_t;

typedef struct proc_info_t {
    int pid;
    int priority;
    proc_state_t state;
    int ticks_left;
    bool fg;
    char name[32];
    uint64_t sp;
    uint64_t bp;
} proc_info_t;

extern pcb_t *current;

void     sched_init(void);
void     sched_start(void);
bool     sched_is_enabled(void);
uint64_t schedule(uint64_t cur_rsp);
int      proc_create(void (*entry)(int, char **), int argc, char **argv,
                    int prio, bool fg, const char *name);
void     proc_exit(int code);
int      proc_block(int pid);
int      proc_unblock(int pid);
void     proc_nice(int pid, int new_prio);
int      proc_kill(int pid);
int      proc_get_foreground_pid(void);
void     proc_set_foreground(int pid);
int      proc_snapshot(proc_info_t *out, int max_items);
int      proc_wait(int pid, int *status);

pcb_t   *proc_by_pid(int pid);
pcb_t   *sched_get_idle(void);
pcb_t   *sched_current(void);
void     sched_enqueue(pcb_t *proc);
void     sched_remove(pcb_t *proc);
void     sched_force_yield(void);

#endif
