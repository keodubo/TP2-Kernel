#include "include/scheduler.h"
#include "include/process.h"
#include <stddef.h>
#include <time.h>

static process_t* current_process = NULL;
static int current_quantum = 0;
static int scheduler_enabled = 0;

static process_t* ready_heads[MAX_PRIORITY + 1] = {0};
static process_t* ready_tails[MAX_PRIORITY + 1] = {0};

static void enqueue_ready(process_t* proc);
static void dequeue_specific(process_t* proc);
static process_t* dequeue_next_ready(void);
extern process_t* process_get_idle();

void scheduler_init() {
    current_process = NULL;
    current_quantum = 0;
    scheduler_enabled = 0;

    for (int i = MIN_PRIORITY; i <= MAX_PRIORITY; i++) {
        ready_heads[i] = NULL;
        ready_tails[i] = NULL;
    }
}

void scheduler_enable() {
    scheduler_enabled = 1;
}

void scheduler_disable() {
    scheduler_enabled = 0;
}

int scheduler_is_enabled() {
    return scheduler_enabled;
}

process_t* scheduler_get_current() {
    return current_process;
}

void scheduler_add_process(process_t* proc) {
    if (proc == NULL) {
        return;
    }

    proc->queue_next = NULL;

    if (scheduler_enabled && current_process == NULL) {
        proc->state = RUNNING;
        current_process = proc;
        current_quantum = QUANTUM_TICKS;
        return;
    }

    if (proc->state != READY) {
        proc->state = READY;
    }

    enqueue_ready(proc);
}

void scheduler_remove_process(process_t* proc) {
    if (proc == NULL) {
        return;
    }
    
    if (proc == current_process) {
        current_process = NULL;
        current_quantum = 0;
    }

    if (proc->state == READY) {
        dequeue_specific(proc);
    }
}

process_t* scheduler_pick_next() {
    process_t* next = dequeue_next_ready();
    if (next == NULL) {
        // No hay procesos listos, usar idle
        next = process_get_idle();
    }
    
    if (next != NULL) {
        next->state = RUNNING;
        current_process = next;
        current_quantum = QUANTUM_TICKS;
    }
    return next;
}

process_t* scheduler_tick() {
    process_collect_zombies();

    if (!scheduler_enabled) {
        return current_process;
    }

    if (current_process == NULL) {
        return scheduler_pick_next();
    }

    // Si el proceso actual esta terminado, no disminuir quantum, solo cambiar
    if (current_process->state == TERMINATED) {
        return scheduler_yield();
    }

    if (current_quantum > 0) {
        current_quantum--;
    }

    if (current_quantum <= 0) {
        return scheduler_yield();
    }

    return current_process;
}

process_t* scheduler_yield() {
    if (!scheduler_enabled) {
        return current_process;
    }

    process_t* old_process = current_process;

    // Si el proceso actual esta corriendo, ponerlo en ready
    if (old_process != NULL && old_process->state == RUNNING) {
        old_process->state = READY;
        enqueue_ready(old_process);
    }
    // Si esta bloqueado o terminado, no lo re-encolar

    process_t* next_process = dequeue_next_ready();
    if (next_process == NULL) {
        // No hay procesos listos
        // Si el viejo proceso todavia puede correr, volver a el
        if (old_process != NULL && old_process->state == READY) {
            old_process->state = RUNNING;
            current_process = old_process;
            current_quantum = QUANTUM_TICKS;
            return current_process;
        }
        // Si no, devolver NULL para que se maneje externamente
        current_process = NULL;
        return NULL;
    }

    next_process->state = RUNNING;
    current_process = next_process;
    current_quantum = QUANTUM_TICKS;

    return next_process;
}

process_context_t* scheduler_handle_timer_tick(uint64_t* stack_ptr) {
    timer_handler();

    if (!scheduler_enabled) {
        return NULL;
    }

    process_t* current = current_process;
    if (current != NULL) {
        process_context_t* ctx = &current->context;

    ctx->r15 = stack_ptr[0];
    ctx->r14 = stack_ptr[1];
    ctx->r13 = stack_ptr[2];
    ctx->r12 = stack_ptr[3];
    ctx->r11 = stack_ptr[4];
    ctx->r10 = stack_ptr[5];
    ctx->r9  = stack_ptr[6];
    ctx->r8  = stack_ptr[7];
    ctx->rsi = stack_ptr[8];
    ctx->rdi = stack_ptr[9];
    ctx->rbp = stack_ptr[10];
    ctx->rdx = stack_ptr[11];
    ctx->rcx = stack_ptr[12];
    ctx->rbx = stack_ptr[13];
    ctx->rax = stack_ptr[14];
    ctx->rip = stack_ptr[15];
    ctx->rflags = stack_ptr[17];
        ctx->rsp = (uint64_t)(stack_ptr + 18);
    }

    process_t* next = scheduler_tick();
    if (next == NULL || next == current) {
        return NULL;
    }

    return &next->context;
}

static void enqueue_ready(process_t* proc) {
    if (proc == NULL) {
        return;
    }

    proc->queue_next = NULL;

    uint8_t priority = proc->priority;
    if (priority < MIN_PRIORITY) {
        priority = MIN_PRIORITY;
    } else if (priority > MAX_PRIORITY) {
        priority = MAX_PRIORITY;
    }

    if (ready_heads[priority] == NULL) {
        ready_heads[priority] = proc;
        ready_tails[priority] = proc;
    } else {
        ready_tails[priority]->queue_next = proc;
        ready_tails[priority] = proc;
    }
}

static void dequeue_specific(process_t* proc) {
    if (proc == NULL) {
        return;
    }

    uint8_t priority = proc->priority;
    if (priority < MIN_PRIORITY || priority > MAX_PRIORITY) {
        return;
    }

    process_t* prev = NULL;
    process_t* cursor = ready_heads[priority];

    while (cursor != NULL) {
        if (cursor == proc) {
            if (prev == NULL) {
                ready_heads[priority] = cursor->queue_next;
            } else {
                prev->queue_next = cursor->queue_next;
            }

            if (ready_tails[priority] == cursor) {
                ready_tails[priority] = prev;
            }

            proc->queue_next = NULL;
            return;
        }
        prev = cursor;
        cursor = cursor->queue_next;
    }
}

static process_t* dequeue_next_ready(void) {
    for (uint8_t priority = MIN_PRIORITY; priority <= MAX_PRIORITY; priority++) {
        process_t* head = ready_heads[priority];
        if (head != NULL) {
            ready_heads[priority] = head->queue_next;
            if (ready_heads[priority] == NULL) {
                ready_tails[priority] = NULL;
            }

            head->queue_next = NULL;
            return head;
        }
    }
    return NULL;
}