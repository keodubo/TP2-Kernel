#include "include/sched.h"
#include "include/memory_manager.h"
#include "include/lib.h"
#include "include/tty.h"
#include "include/fd.h"
#include "include/semaphore.h"
#include "include/syscalls.h"
#include "include/naiveConsole.h"

// Gestion de procesos (PCB)
// Implementa tabla de procesos, creacion, destruccion, comunicacion
// Incluye manejo de zombis, espera y notificaciones entre procesos
// Utilidades para gestion del stack de usuario y trampolines


#define KSTACK_SIZE (16 * 1024)

static pcb_t procs[MAX_PROCS];
static pcb_t *proc_head = NULL;
static pcb_t *proc_tail = NULL;
static pcb_t *zombie_head = NULL;
static int next_pid = 1;

typedef struct wait_result {
    int child_pid;
    int exit_code;
    struct wait_result *next;
} wait_result_t;

static pcb_t *allocate_slot(void);
static void release_slot(pcb_t *proc);
static void setup_stack(pcb_t *proc);
static void insert_proc(pcb_t *proc);
static void remove_proc(pcb_t *proc);
static void enqueue_zombie(pcb_t *proc);
static void collect_zombies(void);
static void copy_name(char *dst, const char *src, size_t max_len);
static void trampoline(pcb_t *proc);
static void notify_parent_exit(pcb_t *proc);
static void link_child(pcb_t *parent, pcb_t *child);
static void unlink_child(pcb_t *parent, pcb_t *child);
static void push_wait_result(pcb_t *parent, int child_pid, int exit_code);
static int consume_wait_result(pcb_t *parent, int target_pid, int *status);
static pcb_t *find_child(pcb_t *parent, int child_pid);
static bool parent_has_children(pcb_t *parent);
static void cleanup_wait_results(pcb_t *proc);
static void detach_children(pcb_t *parent);

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

    // Reservar un slot en la tabla de procesos (hay MAX_PROCS slots disponibles)
    pcb_t *proc = allocate_slot();
    if (proc == NULL) {
        return -1;
    }

    // Inicializar PCB con datos básicos del proceso
    copy_name(proc->name, name, sizeof(proc->name));
    proc->pid = next_pid++;
    if (next_pid <= 0) {
        next_pid = 1;
    }

    proc->priority = prio;
    proc->base_priority = prio;
    proc->aging_ticks = 0;
    proc->state = NEW;
    proc->fg = fg;
    proc->ticks_left = TIME_SLICE_TICKS;  // Quantum inicial
    proc->parent_pid = -1;
    proc->entry = entry;  // Puntero a función de userland
    proc->argc = argc;
    proc->argv = argv;
    
    // Inicializar estructuras para wait/reap
    proc->exit_code = 0;
    proc->waiting_for = 0;
    proc->child_head = NULL;
    proc->sibling_next = NULL;
    proc->waiter_head = NULL;
    proc->wait_res_head = NULL;
    proc->wait_res_tail = NULL;
    proc->pending_exit_pid = -1;
    proc->pending_exit_code = 0;
    proc->pending_exit_valid = false;
    proc->exited = false;
    proc->zombie_reapable = false;
    proc->fd_table = NULL;

    // Obtener proceso padre actual
    pcb_t *parent = sched_current();
    if (parent != NULL) {
        proc->parent_pid = parent->pid;
    }

    // Asignar stack del kernel (16KB) usando el memory manager
    proc->kstack_base = (uint8_t *)mm_malloc(KSTACK_SIZE);
    if (proc->kstack_base == NULL) {
        release_slot(proc);
        return -1;
    }

    // Configurar el stack inicial con los registros para el primer context switch
    setup_stack(proc);

    proc->fd_table = fd_table_create();
    if (proc->fd_table == NULL) {
        mm_free(proc->kstack_base);
        proc->kstack_base = NULL;
        release_slot(proc);
        return -1;
    }

    if (parent != NULL) {
        if (parent->fd_table != NULL) {
            if (fd_table_clone(proc->fd_table, parent->fd_table) < 0) {
                fd_table_destroy(proc->fd_table);
                proc->fd_table = NULL;
                mm_free(proc->kstack_base);
                proc->kstack_base = NULL;
                release_slot(proc);
                return -1;
            }
        } else {
            fd_table_attach_std(proc->fd_table);
        }
        link_child(parent, proc);
    } else {
        fd_table_attach_std(proc->fd_table);
    }
    insert_proc(proc);

    sched_enqueue(proc);

    ncPrint("[KERNEL proc_create] Created process PID=");
    ncPrintDec(proc->pid);
    ncPrint(" prio=");
    ncPrintDec(proc->priority);
    ncNewline();

    // Si el proceso se crea como foreground, actualizar la TTY
    if (fg) {
        tty_set_foreground(proc->pid);
    }

    return proc->pid;
}

void proc_exit(int code) {
    collect_zombies();

    pcb_t *proc = sched_current();
    pcb_t *idle = sched_get_idle();

    if (proc == NULL || proc == idle) {
        return;
    }

    ksem_remove_waiters_for(proc);

    // Liberar todos los handles de semáforos que este proceso abrió
    sem_cleanup_process_handles(proc->pid);

    // Si este proceso era el foreground, liberar la TTY
    if (proc->fg) {
        int parent_pid = proc->parent_pid;
        pcb_t *parent = (parent_pid > 0) ? proc_by_pid(parent_pid) : NULL;
        
        // Devolver el control al padre (típicamente la shell)
        if (parent != NULL && parent->state != EXITED) {
            proc_set_foreground(parent_pid);
        } else {
            // Sin padre válido, liberar la TTY
            tty_set_foreground(-1);
        }
    }

    proc->exit_code = code;
    proc->state = EXITED;
    proc->ticks_left = 0;
    proc->exited = true;
   proc->waiting_for = 0;
   proc->waiter_head = NULL;
   detach_children(proc);
   remove_proc(proc);
   notify_parent_exit(proc);
    if (proc->fd_table != NULL) {
        fd_table_destroy(proc->fd_table);
        proc->fd_table = NULL;
    }
    cleanup_wait_results(proc);
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
        proc->ticks_left = TIME_SLICE_TICKS;
        proc->aging_ticks = 0;
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

    proc->base_priority = new_prio;
    proc->aging_ticks = 0;
    bool was_running = (proc->state == RUNNING && proc == sched_current());
    bool was_ready = (proc->state == READY);

    // Remover de la cola actual si está READY
    if (was_ready) {
        sched_remove(proc);
    } else if (was_running) {
        // Si está RUNNING, cambiar a READY y limpiar current
        // (no está en ninguna cola, solo está marcado como current)
        proc->state = READY;
        if (sched_current() == proc) {
            // Limpiar current para que el scheduler pueda elegir otro proceso
            sched_remove(proc);
        }
    }

    // Cambiar la prioridad
    proc->priority = new_prio;

    // Encolar en la cola correcta según la nueva prioridad
    if (was_ready || was_running) {
        sched_enqueue(proc);
        
        // Si estaba corriendo, forzar un reschedule para que el scheduler
        // elija el proceso con mayor prioridad (que puede no ser este)
        if (was_running) {
            sched_force_yield();
        }
    }
}

int proc_kill(int pid) {
    collect_zombies();

    pcb_t *target = proc_by_pid(pid);
    pcb_t *idle = sched_get_idle();

    if (target == NULL || target == idle) {
        return -1;
    }

    ksem_remove_waiters_for(target);

    // Si el proceso a matar es foreground, devolver control al padre
    // No desbloqueamos manualmente al padre porque notify_parent_exit lo hara
    // automaticamente despues de preparar el resultado del wait
    if (target->fg) {
        int parent_pid = target->parent_pid;
        pcb_t *parent = (parent_pid > 0) ? proc_by_pid(parent_pid) : NULL;
        
        // Devolver el control al padre (tipicamente la shell)
        if (parent != NULL && parent->state != EXITED) {
            // El padre recibira el foreground despues de ser notificado
            proc_set_foreground(parent_pid);
        } else {
            // Sin padre valido o padre muerto, buscar el shell principal
            pcb_t *shell_proc = NULL;
            for (int i = 0; i < MAX_PROCS; i++) {
                if (procs[i].used && procs[i].state != EXITED &&
                    strcmp(procs[i].name, "shell") == 0) {
                    shell_proc = &procs[i];
                    break;
                }
            }
            
            if (shell_proc != NULL) {
                proc_set_foreground(shell_proc->pid);
            } else {
                // Ultimo recurso: liberar la TTY
                tty_set_foreground(-1);
            }
        }
    }

    if (target->state == READY) {
        sched_remove(target);
    } else if (target == sched_current()) {
        proc_exit(0);
        return 0;
    }

    target->exit_code = 0;
    target->state = EXITED;
    target->ticks_left = 0;
    target->exited = true;
    target->waiting_for = 0;
    target->waiter_head = NULL;
    target->pending_exit_valid = false;
    detach_children(target);
    remove_proc(target);
    notify_parent_exit(target);
    if (target->fd_table != NULL) {
        fd_table_destroy(target->fd_table);
        target->fd_table = NULL;
    }
    cleanup_wait_results(target);
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

int proc_get_foreground_pid(void) {
    // Primero consultar la TTY, que es la fuente de verdad
    int tty_fg = tty_get_foreground();
    if (tty_fg > 0) {
        return tty_fg;
    }
    
    // Fallback: buscar algún proceso marcado como fg
    for (int i = 0; i < MAX_PROCS; i++) {
        if (procs[i].used && procs[i].fg && procs[i].state != EXITED) {
            return procs[i].pid;
        }
    }
    return -1;
}

/**
 * @brief Establece un proceso como foreground en la TTY
 * @param pid PID del proceso a poner en foreground (-1 para ninguno)
 */
void proc_set_foreground(int pid) {
    // Marcar el nuevo foreground
    if (pid > 0) {
        pcb_t *proc = proc_by_pid(pid);
        if (proc != NULL) {
            proc->fg = true;
        }
    }
    
    // Actualizar la TTY
    tty_set_foreground(pid);
    
    // Desmarcar todos los demás procesos
    for (int i = 0; i < MAX_PROCS; i++) {
        if (procs[i].used && procs[i].pid != pid) {
            procs[i].fg = false;
        }
    }
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
    if (proc->fd_table != NULL) {
        fd_table_destroy(proc->fd_table);
        proc->fd_table = NULL;
    }
    cleanup_wait_results(proc);
    proc->wait_res_head = NULL;
    proc->wait_res_tail = NULL;
    proc->pending_exit_valid = false;
    proc->child_head = NULL;
    proc->sibling_next = NULL;
    proc->waiter_head = NULL;
    proc->used = false;
}

static void setup_stack(pcb_t *proc) {
    uint64_t top = (uint64_t)proc->kstack_base + KSTACK_SIZE;
    top &= ~0xFULL;  // Align to 16 bytes

    // Save aligned top for RSP calculation
    uint64_t aligned_top = top;

    top -= sizeof(regs_t);

    regs_t *frame = (regs_t *)top;
    for (size_t i = 0; i < sizeof(regs_t) / sizeof(uint64_t); i++) {
        ((uint64_t *)frame)[i] = 0;
    }

    frame->rip = (uint64_t)trampoline;
    frame->cs = 0x08;
    frame->rflags = 0x202;
    // x86-64 ABI: After ret, RSP must be aligned to 16+8 (simulates call)
    frame->rsp = aligned_top - 8;
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
    pcb_t *pending = NULL;

    while (cursor != NULL) {
        pcb_t *next = cursor->cleanup_next;
        cursor->cleanup_next = NULL;

        if (cursor->zombie_reapable || cursor->parent_pid < 0) {
            if (cursor->kstack_base != NULL) {
                mm_free(cursor->kstack_base);
                cursor->kstack_base = NULL;
            }
            cleanup_wait_results(cursor);
            release_slot(cursor);
        } else {
            cursor->cleanup_next = pending;
            pending = cursor;
        }
        cursor = next;
    }
    zombie_head = pending;
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
    ncPrint("[KERNEL trampoline] Entered!\n");
    if (proc == NULL) {
        ncPrint("[KERNEL trampoline] ERROR: proc is NULL\n");
        proc_exit(-1);
        while (1) _hlt();
    }
    if (proc->entry == NULL) {
        ncPrint("[KERNEL trampoline] ERROR: entry is NULL\n");
        proc_exit(-1);
        while (1) _hlt();
    }
    ncPrint("[KERNEL trampoline] Calling entry...\n");
    proc->entry(proc->argc, proc->argv);
    ncPrint("[KERNEL trampoline] Entry returned, exiting...\n");
    proc_exit(0);
    while (1) {
        _hlt();
    }
}

static void notify_parent_exit(pcb_t *proc) {
    if (proc == NULL) {
        return;
    }

    proc->zombie_reapable = true;
    proc->waiter_head = NULL;

    if (proc->parent_pid < 0) {
        return;
    }

    pcb_t *parent = proc_by_pid(proc->parent_pid);
    if (parent == NULL) {
        return;
    }

    unlink_child(parent, proc);
    push_wait_result(parent, proc->pid, proc->exit_code);
    proc->parent_pid = -1;

    if (parent->state == BLOCKED &&
        (parent->waiting_for == proc->pid || parent->waiting_for == -1)) {
        parent->waiting_for = 0;
        proc_unblock(parent->pid);
    }
}

int proc_wait(int target_pid, int *status) {
    collect_zombies();

    pcb_t *parent = sched_current();
    if (parent == NULL) {
        return -1;
    }

    if (target_pid == 0) {
        target_pid = -1;
    }

    int exit_status = 0;
    int waited_pid = consume_wait_result(parent, target_pid, &exit_status);
    if (waited_pid > 0) {
        if (status != NULL) {
            *status = exit_status;
        }
        return waited_pid;
    }

    pcb_t *target_child = NULL;

    if (target_pid > 0) {
        pcb_t *child = find_child(parent, target_pid);
        if (child == NULL) {
            if (parent->pending_exit_valid &&
                parent->pending_exit_pid == target_pid) {
                waited_pid = parent->pending_exit_pid;
                exit_status = parent->pending_exit_code;
                parent->pending_exit_valid = false;
                if (status != NULL) {
                    *status = exit_status;
                }
                return waited_pid;
            }
            return -1;
        }
        child->waiter_head = parent;
        target_child = child;
    } else {
        if (!parent_has_children(parent)) {
            if (parent->pending_exit_valid) {
                waited_pid = parent->pending_exit_pid;
                exit_status = parent->pending_exit_code;
                parent->pending_exit_valid = false;
                if (status != NULL) {
                    *status = exit_status;
                }
                return waited_pid;
            }
            return -1;
        }
    }

    parent->waiting_for = target_pid;
    parent->state = BLOCKED;
    parent->ticks_left = 0;
    sched_force_yield();

    collect_zombies();

    waited_pid = consume_wait_result(parent, target_pid, &exit_status);
    parent->waiting_for = 0;
    
    if (target_child != NULL) {
        target_child->waiter_head = NULL;
    }

    if (waited_pid > 0) {
        if (status != NULL) {
            *status = exit_status;
        }
        return waited_pid;
    }

    if (parent->pending_exit_valid &&
        (target_pid <= 0 || parent->pending_exit_pid == target_pid)) {
        waited_pid = parent->pending_exit_pid;
        exit_status = parent->pending_exit_code;
        parent->pending_exit_valid = false;
        if (status != NULL) {
            *status = exit_status;
        }
        return waited_pid;
    }

    return -1;
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
        info->sp = 0;
        info->bp = 0;
        if (procs[i].kframe != NULL) {
            info->sp = procs[i].kframe->rsp;
            info->bp = procs[i].kframe->rbp;
        }
        count++;
    }

    return count;
}

static void link_child(pcb_t *parent, pcb_t *child) {
    if (parent == NULL || child == NULL) {
        return;
    }
    child->sibling_next = parent->child_head;
    parent->child_head = child;
}

static void unlink_child(pcb_t *parent, pcb_t *child) {
    if (parent == NULL || child == NULL) {
        return;
    }

    pcb_t *prev = NULL;
    pcb_t *cursor = parent->child_head;
    while (cursor != NULL) {
        if (cursor == child) {
            if (prev == NULL) {
                parent->child_head = cursor->sibling_next;
            } else {
                prev->sibling_next = cursor->sibling_next;
            }
            child->sibling_next = NULL;
            return;
        }
        prev = cursor;
        cursor = cursor->sibling_next;
    }
}

static pcb_t *find_child(pcb_t *parent, int child_pid) {
    if (parent == NULL || child_pid <= 0) {
        return NULL;
    }

    pcb_t *cursor = parent->child_head;
    while (cursor != NULL) {
        if (cursor->pid == child_pid) {
            return cursor;
        }
        cursor = cursor->sibling_next;
    }
    return NULL;
}

static bool parent_has_children(pcb_t *parent) {
    return parent != NULL && parent->child_head != NULL;
}

static void push_wait_result(pcb_t *parent, int child_pid, int exit_code) {
    if (parent == NULL) {
        return;
    }

    wait_result_t *node = (wait_result_t *)mm_malloc(sizeof(wait_result_t));
    if (node == NULL) {
        parent->pending_exit_pid = child_pid;
        parent->pending_exit_code = exit_code;
        parent->pending_exit_valid = true;
        return;
    }

    node->child_pid = child_pid;
    node->exit_code = exit_code;
    node->next = NULL;

    if (parent->wait_res_tail == NULL) {
        parent->wait_res_head = parent->wait_res_tail = node;
    } else {
        parent->wait_res_tail->next = node;
        parent->wait_res_tail = node;
    }
}

static int consume_wait_result(pcb_t *parent, int target_pid, int *status) {
    if (parent == NULL) {
        return -1;
    }

    wait_result_t *prev = NULL;
    wait_result_t *node = parent->wait_res_head;

    if (target_pid <= 0) {
        if (node != NULL) {
            parent->wait_res_head = node->next;
            if (parent->wait_res_tail == node) {
                parent->wait_res_tail = NULL;
            }
            int pid = node->child_pid;
            if (status != NULL) {
                *status = node->exit_code;
            }
            mm_free(node);
            return pid;
        }
    } else {
        while (node != NULL && node->child_pid != target_pid) {
            prev = node;
            node = node->next;
        }
        if (node != NULL) {
            if (prev == NULL) {
                parent->wait_res_head = node->next;
            } else {
                prev->next = node->next;
            }
            if (parent->wait_res_tail == node) {
                parent->wait_res_tail = prev;
            }
            int pid = node->child_pid;
            if (status != NULL) {
                *status = node->exit_code;
            }
            mm_free(node);
            return pid;
        }
    }

    if (parent->pending_exit_valid &&
        (target_pid <= 0 || parent->pending_exit_pid == target_pid)) {
        int pid = parent->pending_exit_pid;
        if (status != NULL) {
            *status = parent->pending_exit_code;
        }
        parent->pending_exit_valid = false;
        return pid;
    }

    return -1;
}

static void cleanup_wait_results(pcb_t *proc) {
    if (proc == NULL) {
        return;
    }

    wait_result_t *node = proc->wait_res_head;
    while (node != NULL) {
        wait_result_t *next = node->next;
        mm_free(node);
        node = next;
    }
    proc->wait_res_head = NULL;
    proc->wait_res_tail = NULL;
    proc->pending_exit_valid = false;
    proc->pending_exit_pid = -1;
    proc->pending_exit_code = 0;
}

static void detach_children(pcb_t *parent) {
    if (parent == NULL) {
        return;
    }

    pcb_t *child = parent->child_head;
    parent->child_head = NULL;

    while (child != NULL) {
        pcb_t *next = child->sibling_next;
        child->sibling_next = NULL;
        
        // Si el hijo todavía está vivo, matarlo
        if (child->state != EXITED) {
            // Remover de scheduler si está en READY
            if (child->state == READY) {
                sched_remove(child);
            }
            
            // Marcar como terminado
            child->exit_code = 0;
            child->state = EXITED;
            child->ticks_left = 0;
            child->exited = true;
            child->waiting_for = 0;
            child->waiter_head = NULL;
            child->pending_exit_valid = false;
            child->parent_pid = -1;
            
            // No llamar recursivamente a detach_children para evitar loops
            // pero sí limpiar sus hijos
            pcb_t *grandchild = child->child_head;
            child->child_head = NULL;
            while (grandchild != NULL) {
                pcb_t *next_gc = grandchild->sibling_next;
                grandchild->sibling_next = NULL;
                grandchild->parent_pid = -1;
                if (grandchild->waiter_head == child) {
                    grandchild->waiter_head = NULL;
                }
                grandchild = next_gc;
            }
            
            // Remover de la lista de procesos y agregar a zombies
            remove_proc(child);
            cleanup_wait_results(child);
            enqueue_zombie(child);
        } else {
            // El hijo ya está muerto, solo desenlazar
            if (child->parent_pid == parent->pid) {
                child->parent_pid = -1;
            }
            if (child->waiter_head == parent) {
                child->waiter_head = NULL;
            }
        }
        
        child = next;
    }
}
