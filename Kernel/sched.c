#include "include/sched.h"

// Archivo: sched.c
// Propósito: Implementación del planificador de procesos del kernel
// Resumen: Colas por prioridad, selección del siguiente proceso, manejo
//          del proceso idle y API para encolar/desencolar procesos.

static pcb_t *ready_head[MAX_PRIOS];
static pcb_t *ready_tail[MAX_PRIOS];
static bool scheduler_enabled = false;

#define FG_SKIP_THRESHOLD 5

pcb_t *current = NULL;

static pcb_t *idle_proc = NULL;
static int fg_skip_counter = 0;

extern void _force_schedule(void);
extern void _hlt(void);

static void q_push(pcb_t *proc);
static pcb_t *q_pop(int prio);
static void q_remove(pcb_t *proc);
static pcb_t *pick_next(void);
static void idle_loop(int argc, char **argv);

void sched_init(void) {
    for (int i = 0; i < MAX_PRIOS; i++) {
        ready_head[i] = NULL;
        ready_tail[i] = NULL;
    }

    scheduler_enabled = false;
    current = NULL;
    idle_proc = NULL;
    fg_skip_counter = 0;

    int idle_pid = proc_create(idle_loop, 0, NULL, MIN_PRIO, false, "idle");
    if (idle_pid >= 0) {
        idle_proc = proc_by_pid(idle_pid);
        if (idle_proc != NULL) {
            idle_proc->fg = false;
            // Remover el idle de las colas de ready
            // El idle solo debe ejecutarse cuando no hay otros procesos
            sched_remove(idle_proc);
            idle_proc->state = RUNNING;
            idle_proc->ticks_left = TIME_SLICE_TICKS;
            current = idle_proc;
        }
    }
}

void sched_start(void) {
    scheduler_enabled = true;
}

bool sched_is_enabled(void) {
    return scheduler_enabled;
}

pcb_t *sched_current(void) {
    return current;
}

pcb_t *sched_get_idle(void) {
    return idle_proc;
}

void sched_enqueue(pcb_t *proc) {
    if (proc == NULL) {
        return;
    }

    if (!scheduler_enabled && current == NULL) {
        current = proc;
        proc->state = RUNNING;
        proc->ticks_left = TIME_SLICE_TICKS;
        return;
    }

    if (proc->priority < MIN_PRIO) {
        proc->priority = MIN_PRIO;
    } else if (proc->priority > HIGHEST_PRIO) {
        proc->priority = HIGHEST_PRIO;
    }

    proc->state = READY;
    proc->ticks_left = TIME_SLICE_TICKS;
    q_push(proc);
}

void sched_remove(pcb_t *proc) {
    if (proc == NULL) {
        return;
    }

    if (proc == current) {
        current = NULL;
    }

    if (proc->state == READY) {
        q_remove(proc);
    }
}

pcb_t *sched_pick_next(void) {
    return pick_next();
}

void sched_force_yield(void) {
    if (current != NULL) {
        current->ticks_left = 0;
    }
    _force_schedule();
}

uint64_t schedule(uint64_t cur_rsp) {
    if (!scheduler_enabled || current == NULL) {
        return 0;
    }

    current->kframe = (regs_t *)cur_rsp;

    if (current->ticks_left > 0) {
        current->ticks_left--;
    }

    if (current->ticks_left > 0 && current->state == RUNNING) {
        return 0;
    }

    pcb_t *prev = current;

    // El idle process nunca se encola, siempre queda como fallback
    if (prev->state == RUNNING && prev != idle_proc) {
        prev->state = READY;
        prev->ticks_left = TIME_SLICE_TICKS;
        q_push(prev);
    }

    if (prev->state == BLOCKED || prev->state == EXITED) {
        prev->ticks_left = 0;
    }

    pcb_t *next = pick_next();
    if (next == NULL) {
        next = idle_proc;
    }

    if (next == NULL) {
        current = prev;
        return 0;
    }

    next->state = RUNNING;
    next->ticks_left = TIME_SLICE_TICKS;
    if (next->fg) {
        fg_skip_counter = 0;
    }
    current = next;

    return (uint64_t)current->kframe;
}

static void q_push(pcb_t *proc) {
    int prio = proc->priority;
    if (prio < MIN_PRIO) {
        prio = MIN_PRIO;
    } else if (prio > HIGHEST_PRIO) {
        prio = HIGHEST_PRIO;
    }

    proc->queue_next = NULL;

    if (ready_head[prio] == NULL) {
        ready_head[prio] = proc;
        ready_tail[prio] = proc;
    } else {
        ready_tail[prio]->queue_next = proc;
        ready_tail[prio] = proc;
    }
}

static pcb_t *q_pop(int prio) {
    pcb_t *head = ready_head[prio];
    if (head == NULL) {
        return NULL;
    }

    ready_head[prio] = head->queue_next;
    if (ready_head[prio] == NULL) {
        ready_tail[prio] = NULL;
    }

    head->queue_next = NULL;
    return head;
}

static void q_remove(pcb_t *proc) {
    int prio = proc->priority;
    if (prio < MIN_PRIO || prio > HIGHEST_PRIO) {
        return;
    }

    pcb_t *cursor = ready_head[prio];
    pcb_t *prev = NULL;

    while (cursor != NULL) {
        if (cursor == proc) {
            if (prev == NULL) {
                ready_head[prio] = cursor->queue_next;
            } else {
                prev->queue_next = cursor->queue_next;
            }
            if (ready_tail[prio] == cursor) {
                ready_tail[prio] = prev;
            }
            cursor->queue_next = NULL;
            return;
        }
        prev = cursor;
        cursor = cursor->queue_next;
    }
}

static pcb_t *pick_next(void) {
    pcb_t *fg_candidate = NULL;
    int fg_candidate_prio = MIN_PRIO;
    int highest_ready_prio = -1;

    for (int prio = HIGHEST_PRIO; prio >= MIN_PRIO; prio--) {
        if (ready_head[prio] != NULL && highest_ready_prio < prio) {
            highest_ready_prio = prio;
        }

        if (fg_candidate == NULL) {
            pcb_t *cursor = ready_head[prio];
            while (cursor != NULL) {
                if (cursor->fg) {
                    fg_candidate = cursor;
                    fg_candidate_prio = prio;
                    break;
                }
                cursor = cursor->queue_next;
            }
        }
    }

    bool fg_available = (fg_candidate != NULL);

    if (!fg_available) {
        fg_skip_counter = 0;
    }

    if (fg_available && highest_ready_prio > fg_candidate_prio) {
        if (fg_skip_counter >= FG_SKIP_THRESHOLD) {
            fg_skip_counter = 0;
            q_remove(fg_candidate);
            return fg_candidate;
        }
    } else if (fg_available && highest_ready_prio == fg_candidate_prio) {
        fg_skip_counter = 0;
    }

    // Selección estricta por prioridad para el resto
    for (int prio = HIGHEST_PRIO; prio >= MIN_PRIO; prio--) {
        if (ready_head[prio] != NULL) {
            pcb_t *next = q_pop(prio);
            if (next != NULL) {
                if (next->fg) {
                    fg_skip_counter = 0;
                } else if (fg_available && highest_ready_prio > fg_candidate_prio) {
                    fg_skip_counter++;
                } else if (!fg_available) {
                    fg_skip_counter = 0;
                }
                return next;
            }
        }
    }

    return NULL;
}

static void idle_loop(int argc, char **argv) {
    (void)argc;
    (void)argv;

    while (1) {
        _hlt();
    }
}
