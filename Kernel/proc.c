#include "include/sched.h"
#include "include/memory_manager.h"
#include "include/lib.h"

#define KSTACK_SIZE (16 * 1024)

static pcb_t procs[MAX_PROCS];
static pcb_t *proc_head = NULL;
static pcb_t *proc_tail = NULL;
static pcb_t *zombie_head = NULL;
static int next_pid = 1;

static pcb_t *allocate_slot(void);
static void release_slot(pcb_t *proc);
static void setup_stack(pcb_t *proc);
static void insert_proc(pcb_t *proc);
static void remove_proc(pcb_t *proc);
static void enqueue_zombie(pcb_t *proc);
static void collect_zombies(void);
static void copy_name(char *dst, const char *src, size_t max_len);
static void trampoline(pcb_t *proc);
static bool has_active_children(int parent_pid);
static void notify_parent_exit(pcb_t *proc);

extern void _hlt(void);

int proc_create(void (*entry)(int, char **), int argc, char **argv,
                int prio, bool fg, const char *name) {
    if (entry == NULL || name == NULL) {
        return -1;
    }

    if (prio < MIN_PRIO) {
        prio = MIN_PRIO;
    } else if (prio > HIGHEST_PRIO) {
        prio = HIGHEST_PRIO;
    }

    collect_zombies();

    pcb_t *proc = allocate_slot();
    if (proc == NULL) {
        return -1;
    }

    copy_name(proc->name, name, sizeof(proc->name));

    proc->pid = next_pid++;
    if (next_pid <= 0) {
        next_pid = 1;
    }

    proc->priority = prio;
    proc->state = NEW;
    proc->fg = fg;
    proc->ticks_left = TIME_SLICE_TICKS;
    proc->parent_pid = -1;
    proc->entry = entry;
    proc->argc = argc;
    proc->argv = argv;
    proc->exit_code = 0;

    pcb_t *parent = sched_current();
    if (parent != NULL) {
        proc->parent_pid = parent->pid;
    }

    proc->kstack_base = (uint8_t *)mm_malloc(KSTACK_SIZE);
    if (proc->kstack_base == NULL) {
        release_slot(proc);
        return -1;
    }

    setup_stack(proc);
    insert_proc(proc);

    sched_enqueue(proc);

    return proc->pid;
}

void proc_exit(int code) {
    collect_zombies();

    pcb_t *proc = sched_current();
    pcb_t *idle = sched_get_idle();

    if (proc == NULL || proc == idle) {
        return;
    }

    proc->exit_code = code;
    proc->state = EXITED;
    proc->ticks_left = 0;

    remove_proc(proc);
    notify_parent_exit(proc);
    enqueue_zombie(proc);

    sched_force_yield();

    while (1) {
        _hlt();
    }
}

int proc_block(int pid) {
    collect_zombies();

    pcb_t *target = NULL;
    if (pid > 0) {
        target = proc_by_pid(pid);
    } else {
        target = sched_current();
    }

    pcb_t *idle = sched_get_idle();

    if (target == NULL || target == idle || target->state == EXITED) {
        return -1;
    }

    if (target->state == BLOCKED) {
        return 0;
    }

    if (target != sched_current()) {
        if (target->state == READY) {
            sched_remove(target);
        }
        target->state = BLOCKED;
        target->ticks_left = 0;
        return 0;
    }

    target->state = BLOCKED;
    target->ticks_left = 0;
    sched_force_yield();
    return 0;
}

int proc_unblock(int pid) {
    collect_zombies();

    pcb_t *proc = proc_by_pid(pid);
    if (proc == NULL || proc->state == EXITED) {
        return -1;
    }

    if (proc->state == BLOCKED) {
        proc->state = READY;
        sched_enqueue(proc);
        return 0;
    }

    return -1;
}

void proc_nice(int pid, int new_prio) {
    collect_zombies();

    if (new_prio < MIN_PRIO) {
        new_prio = MIN_PRIO;
    } else if (new_prio > HIGHEST_PRIO) {
        new_prio = HIGHEST_PRIO;
    }

    pcb_t *proc = proc_by_pid(pid);
    if (proc == NULL) {
        return;
    }

    if (proc->priority == new_prio) {
        return;
    }

    proc->priority = new_prio;

    if (proc->state == READY) {
        sched_remove(proc);
        sched_enqueue(proc);
    } else if (proc == sched_current()) {
        proc->ticks_left = 0;
    }
}

int proc_kill(int pid) {
    collect_zombies();

    pcb_t *target = proc_by_pid(pid);
    pcb_t *idle = sched_get_idle();

    if (target == NULL || target == idle) {
        return -1;
    }

    if (target->state == READY) {
        sched_remove(target);
    } else if (target == sched_current()) {
        proc_exit(0);
        return 0;
    }

    target->state = EXITED;
    target->ticks_left = 0;
    remove_proc(target);
    notify_parent_exit(target);
    enqueue_zombie(target);
    collect_zombies();

    return 0;
}

pcb_t *proc_by_pid(int pid) {
    if (pid <= 0) {
        return NULL;
    }

    for (int i = 0; i < MAX_PROCS; i++) {
        if (procs[i].used && procs[i].pid == pid) {
            return &procs[i];
        }
    }

    return NULL;
}

static pcb_t *allocate_slot(void) {
    for (int i = 0; i < MAX_PROCS; i++) {
        if (!procs[i].used) {
            memset(&procs[i], 0, sizeof(pcb_t));
            procs[i].used = true;
            return &procs[i];
        }
    }
    return NULL;
}

static void release_slot(pcb_t *proc) {
    if (proc == NULL) {
        return;
    }
    proc->used = false;
}

static void setup_stack(pcb_t *proc) {
    uint64_t top = (uint64_t)proc->kstack_base + KSTACK_SIZE;
    top &= ~0xFULL;
    top -= sizeof(regs_t);

    regs_t *frame = (regs_t *)top;
    for (size_t i = 0; i < sizeof(regs_t) / sizeof(uint64_t); i++) {
        ((uint64_t *)frame)[i] = 0;
    }

    frame->rip = (uint64_t)trampoline;
    frame->cs = 0x08;
    frame->rflags = 0x202;
    frame->rsp = (uint64_t)proc->kstack_base + KSTACK_SIZE - 16;
    frame->ss = 0x00;
    frame->rdi = (uint64_t)proc;
    frame->rbp = frame->rsp;

    proc->kframe = frame;
}

static void insert_proc(pcb_t *proc) {
    proc->next = NULL;
    proc->prev = proc_tail;

    if (proc_head == NULL) {
        proc_head = proc;
    } else if (proc_tail != NULL) {
        proc_tail->next = proc;
    }

    proc_tail = proc;
}

static void remove_proc(pcb_t *proc) {
    if (proc == NULL) {
        return;
    }

    if (proc->prev != NULL) {
        proc->prev->next = proc->next;
    } else {
        proc_head = proc->next;
    }

    if (proc->next != NULL) {
        proc->next->prev = proc->prev;
    } else {
        proc_tail = proc->prev;
    }

    proc->next = NULL;
    proc->prev = NULL;
}

static void enqueue_zombie(pcb_t *proc) {
    proc->cleanup_next = zombie_head;
    zombie_head = proc;
}

static void collect_zombies(void) {
    pcb_t *cursor = zombie_head;
    zombie_head = NULL;

    while (cursor != NULL) {
        pcb_t *next = cursor->cleanup_next;
        cursor->cleanup_next = NULL;

        if (cursor->kstack_base != NULL) {
            mm_free(cursor->kstack_base);
            cursor->kstack_base = NULL;
        }

        release_slot(cursor);
        cursor = next;
    }
}

static void copy_name(char *dst, const char *src, size_t max_len) {
    if (dst == NULL || max_len == 0) {
        return;
    }

    size_t i = 0;
    if (src != NULL) {
        for (; i + 1 < max_len && src[i] != '\0'; i++) {
            dst[i] = src[i];
        }
    }
    dst[i] = '\0';
}

static void trampoline(pcb_t *proc) {
    if (proc != NULL && proc->entry != NULL) {
        proc->entry(proc->argc, proc->argv);
    }
    proc_exit(0);
    while (1) {
        _hlt();
    }
}

static bool has_active_children(int parent_pid) {
    for (int i = 0; i < MAX_PROCS; i++) {
        if (procs[i].used && procs[i].parent_pid == parent_pid &&
            procs[i].state != EXITED) {
            return true;
        }
    }
    return false;
}

static void notify_parent_exit(pcb_t *proc) {
    if (proc == NULL || proc->parent_pid < 0) {
        return;
    }

    pcb_t *parent = proc_by_pid(proc->parent_pid);
    if (parent == NULL) {
        return;
    }

    if (!has_active_children(parent->pid) && parent->state == BLOCKED) {
        proc_unblock(parent->pid);
    }
}

int proc_snapshot(proc_info_t *out, int max_items) {
    if (out == NULL || max_items <= 0) {
        return 0;
    }

    collect_zombies();

    int count = 0;
    for (int i = 0; i < MAX_PROCS && count < max_items; i++) {
        if (!procs[i].used) {
            continue;
        }

        proc_info_t *info = &out[count];
        info->pid = procs[i].pid;
        info->priority = procs[i].priority;
        info->state = procs[i].state;
        info->ticks_left = procs[i].ticks_left;
        info->fg = procs[i].fg;
        copy_name(info->name, procs[i].name, sizeof(info->name));
        count++;
    }

    return count;
}
