#include "include/process.h"
#include "include/memory_manager.h"
#include "include/scheduler.h"
#include "include/lib.h"
#include <stddef.h>

static process_t* process_list_head = NULL;
static process_t* process_list_tail = NULL;
static int next_pid = 1;
static int active_processes = 0;
static process_t* idle_process = NULL;
static process_t* terminated_head = NULL;

static void process_setup_stack(process_t* proc);
static void process_insert(process_t* proc);
static void process_remove_from_list(process_t* proc);
static void copy_process_name(char* dest, const char* src, size_t max_len);
static void process_entry_trampoline(process_t* proc);
static void idle_process_entry(int argc, char** argv);
static void process_queue_terminated(process_t* proc);
static void process_notify_parent_exit(process_t* proc);
static int process_has_active_children(int parent_pid);

extern void _hlt();
extern void context_switch(process_context_t* old_context, process_context_t* new_context);

void process_init() {
    process_list_head = NULL;
    process_list_tail = NULL;
    next_pid = 1;
    active_processes = 0;
    terminated_head = NULL;

    scheduler_init();

    int idle_pid = process_create(idle_process_entry, 0, NULL, "idle", MAX_PRIORITY, 0);
    if (idle_pid >= 0) {
        idle_process = process_get_by_pid(idle_pid);
        if (idle_process != NULL) {
            idle_process->is_foreground = 0;
        }
    }
}

void process_start() {
    if (scheduler_is_enabled()) {
        return;
    }

    scheduler_enable();

    if (scheduler_get_current() == NULL) {
        process_t* next = scheduler_pick_next();
        if (next != NULL) {
            context_switch(NULL, &next->context);
        }
    }
}

int process_create(void (*entry_point)(int, char**), int argc, char** argv,
                   const char* name, uint8_t priority, int foreground) {

    if (entry_point == NULL || name == NULL) {
        return -1;
    }

    if (priority < MIN_PRIORITY || priority > MAX_PRIORITY) {
        priority = DEFAULT_PRIORITY;
    }

    process_t* proc = (process_t*)mm_malloc(sizeof(process_t));
    if (proc == NULL) {
        return -1;
    }

    void* stack = mm_malloc(PROCESS_STACK_SIZE);
    if (stack == NULL) {
        mm_free(proc);
        return -1;
    }

    proc->pid = next_pid++;
    copy_process_name(proc->name, name, sizeof(proc->name) - 1);
    proc->state = READY;
    proc->priority = priority;
    proc->stack_base = stack;
    proc->stack_top = (void*)((uint64_t)stack + PROCESS_STACK_SIZE);
    proc->entry_point = entry_point;
    proc->argc = argc;
    proc->argv = argv;
    proc->parent_pid = -1;

    process_t* current = scheduler_get_current();
    if (current != NULL) {
        proc->parent_pid = current->pid;
    }

    proc->is_foreground = foreground ? 1 : 0;
    proc->quantum_count = 0;
    proc->waiting_children = 0;
    proc->exit_status = 0;
    proc->next = NULL;
    proc->prev = NULL;
    proc->queue_next = NULL;
    proc->cleanup_next = NULL;

    memset(&proc->context, 0, sizeof(process_context_t));
    process_setup_stack(proc);

    process_insert(proc);
    active_processes++;

    scheduler_add_process(proc);

    return proc->pid;
}

process_t* process_get_current() {
    return scheduler_get_current();
}

process_t* process_get_by_pid(int pid) {
    process_t* cursor = process_list_head;
    while (cursor != NULL) {
        if (cursor->pid == pid) {
            return cursor;
        }
        cursor = cursor->next;
    }
    return NULL;
}

int process_kill(int pid) {
    process_t* proc = process_get_by_pid(pid);
    if (proc == NULL) {
        return -1;
    }

    if (proc == idle_process) {
        return -1;
    }

    if (proc->state == TERMINATED) {
        return 0;
    }

    process_t* current = scheduler_get_current();

    if (proc == current) {
        process_exit_current();
        return 0;
    }

    if (proc->state == READY) {
        scheduler_remove_process(proc);
    }

    proc->state = TERMINATED;
    process_remove_from_list(proc);
    active_processes--;
    process_notify_parent_exit(proc);
    process_queue_terminated(proc);
    process_collect_zombies();

    return 0;
}

int process_set_priority(int pid, uint8_t new_priority) {
    if (new_priority < MIN_PRIORITY || new_priority > MAX_PRIORITY) {
        return -1;
    }

    process_t* proc = process_get_by_pid(pid);
    if (proc == NULL) {
        return -1;
    }

    proc->priority = new_priority;
    if (proc->state == READY) {
        scheduler_remove_process(proc);
        scheduler_add_process(proc);
    }
    return 0;
}

int process_block(int pid) {
    process_t* proc = process_get_by_pid(pid);
    if (proc == NULL || proc == idle_process) {
        return -1;
    }

    if (proc->state == READY) {
        scheduler_remove_process(proc);
    }

    if (proc->state == RUNNING || proc->state == READY) {
        process_t* current = scheduler_get_current();
        proc->state = BLOCKED;

        if (proc == current) {
            process_t* next = scheduler_yield();
            if (next != NULL && next != current) {
                context_switch(&current->context, &next->context);
            }
        }
        return 0;
    }

    return -1;
}

int process_unblock(int pid) {
    process_t* proc = process_get_by_pid(pid);
    if (proc == NULL || proc == idle_process) {
        return -1;
    }

    if (proc->state == BLOCKED) {
        proc->state = READY;
        scheduler_add_process(proc);
        return 0;
    }

    return -1;
}

void process_yield() {
    process_collect_zombies();
    process_t* current = scheduler_get_current();
    process_t* next = scheduler_yield();

    if (next != NULL && next != current) {
        context_switch(current ? &current->context : NULL, &next->context);
    }
}

int process_count() {
    return active_processes;
}

process_t** process_get_all(int* count) {
    if (count == NULL || active_processes == 0) {
        if (count != NULL) {
            *count = 0;
        }
        return NULL;
    }

    process_t** array = (process_t**)mm_malloc(sizeof(process_t*) * active_processes);
    if (array == NULL) {
        if (count != NULL) {
            *count = 0;
        }
        return NULL;
    }

    int idx = 0;
    process_t* cursor = process_list_head;
    while (cursor != NULL && idx < active_processes) {
        array[idx++] = cursor;
        cursor = cursor->next;
    }

    *count = idx;
    return array;
}

void process_wait_children() {
    process_collect_zombies();
    process_t* current = scheduler_get_current();
    if (current == NULL) {
        return;
    }

    if (!process_has_active_children(current->pid)) {
        return;
    }

    current->waiting_children = 1;
    if (process_block(current->pid) != 0) {
        current->waiting_children = 0;
    }
}

void process_exit_current() {
    process_t* current = scheduler_get_current();
    if (current == NULL || current == idle_process) {
        return;
    }

    scheduler_remove_process(current);
    current->state = TERMINATED;
    process_remove_from_list(current);
    active_processes--;
    process_notify_parent_exit(current);
    process_queue_terminated(current);

    process_t* next = scheduler_yield();

    if (next == NULL || next == current) {
        while (1) {
            _hlt();
        }
    }

    context_switch(NULL, &next->context);

    while (1) {
        _hlt();
    }
}

static void process_setup_stack(process_t* proc) {
    uint64_t* sp = (uint64_t*)proc->stack_top;
    sp = (uint64_t*)((uint64_t)sp & ~0xF);

    proc->context.rip = (uint64_t)process_entry_trampoline;
    proc->context.rsp = (uint64_t)sp;
    proc->context.rbp = (uint64_t)sp;
    proc->context.rflags = 0x202;
    proc->context.rdi = (uint64_t)proc;
}

static void process_insert(process_t* proc) {
    proc->next = NULL;
    proc->prev = process_list_tail;

    if (process_list_head == NULL) {
        process_list_head = proc;
    } else if (process_list_tail != NULL) {
        process_list_tail->next = proc;
    }

    process_list_tail = proc;
}

static void process_remove_from_list(process_t* proc) {
    if (proc == NULL) {
        return;
    }

    if (proc->prev != NULL) {
        proc->prev->next = proc->next;
    } else {
        process_list_head = proc->next;
    }

    if (proc->next != NULL) {
        proc->next->prev = proc->prev;
    } else {
        process_list_tail = proc->prev;
    }

    proc->next = NULL;
    proc->prev = NULL;
}

static void copy_process_name(char* dest, const char* src, size_t max_len) {
    if (dest == NULL || max_len == 0) {
        return;
    }

    size_t i;
    for (i = 0; i < max_len && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

static void process_entry_trampoline(process_t* proc) {
    if (proc != NULL && proc->entry_point != NULL) {
        proc->state = RUNNING;
        proc->entry_point(proc->argc, proc->argv);
    }
    process_exit_current();
}

static void idle_process_entry(int argc, char** argv) {
    (void)argc;
    (void)argv;

    while (1) {
        _hlt();
    }
}

void process_collect_zombies() {
    process_t* cursor = terminated_head;
    terminated_head = NULL;

    while (cursor != NULL) {
        process_t* next = cursor->cleanup_next;
        cursor->cleanup_next = NULL;

        if (cursor->stack_base != NULL) {
            mm_free(cursor->stack_base);
        }
        mm_free(cursor);

        cursor = next;
    }
}

static void process_queue_terminated(process_t* proc) {
    if (proc == NULL) {
        return;
    }

    proc->cleanup_next = terminated_head;
    terminated_head = proc;
}

static int process_has_active_children(int parent_pid) {
    process_t* cursor = process_list_head;
    while (cursor != NULL) {
        if (cursor->parent_pid == parent_pid && cursor->state != TERMINATED) {
            return 1;
        }
        cursor = cursor->next;
    }
    return 0;
}

static void process_notify_parent_exit(process_t* proc) {
    if (proc == NULL || proc->parent_pid < 0) {
        return;
    }

    process_t* parent = process_get_by_pid(proc->parent_pid);
    if (parent == NULL) {
        return;
    }

    if (parent->waiting_children && !process_has_active_children(parent->pid)) {
        parent->waiting_children = 0;
        if (parent->state == BLOCKED) {
            process_unblock(parent->pid);
        }
    }
}