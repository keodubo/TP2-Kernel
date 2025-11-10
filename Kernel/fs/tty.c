#include <stddef.h>
#include "tty.h"
#include "memory_manager.h"
#include "interrupts.h"
#include "lib.h"
#include "sched.h"
#include "videoDriver.h"

// Backend de la TTY principal: buffer circular + colas de procesos bloqueados

#define TTY_BUFFER_CAP 256

typedef struct tty_waiter {
    pcb_t *proc;
    struct tty_waiter *next;
} tty_waiter_t;

struct tty {
    char buffer[TTY_BUFFER_CAP];
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    bool eof;
    tty_waiter_t *wait_head;
    tty_waiter_t *wait_tail;
    int fg_pid;  // PID del proceso foreground que controla la TTY
};

static tty_t default_tty;
static bool default_tty_initialized = false;

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

static void enqueue_waiter(tty_t *t, pcb_t *proc, tty_waiter_t *node) {
    if (node == NULL) {
        return;
    }
    node->proc = proc;
    node->next = NULL;

    if (t->wait_tail == NULL) {
        t->wait_head = t->wait_tail = node;
    } else {
        t->wait_tail->next = node;
        t->wait_tail = node;
    }
}

static pcb_t *dequeue_waiter(tty_t *t) {
    if (t->wait_head == NULL) {
        return NULL;
    }

    tty_waiter_t *node = t->wait_head;
    t->wait_head = node->next;
    if (t->wait_head == NULL) {
        t->wait_tail = NULL;
    }

    pcb_t *proc = node->proc;
    mm_free(node);
    return proc;
}

// Devuelve la instancia singleton de la consola
tty_t *tty_default(void) {
    if (!default_tty_initialized) {
        memset(&default_tty, 0, sizeof(default_tty));
        default_tty.fg_pid = -1;  // Inicialmente sin foreground
        default_tty_initialized = true;
    }
    return &default_tty;
}

// Bloqueante: consume datos del buffer o espera a que lleguen
int tty_read(tty_t *t, void *buf, int n) {
    if (t == NULL || buf == NULL || n <= 0) {
        return -1;
    }

    char *dst = (char *)buf;
    int total = 0;

    while (total < n) {
        uint64_t flags = irq_save_local();

        if (t->size > 0) {
            int to_read = (t->size < (uint32_t)(n - total)) ? (int)t->size : (n - total);
            for (int i = 0; i < to_read; i++) {
                dst[total++] = t->buffer[t->head];
                t->head = (t->head + 1) % TTY_BUFFER_CAP;
                t->size--;
            }

            irq_restore_local(flags);

            if (total > 0) {
                return total;
            }
            continue;
        }

        if (t->eof) {
            t->eof = false;
            irq_restore_local(flags);
            return total;
        }

        pcb_t *current = sched_current();
        if (current == NULL) {
            irq_restore_local(flags);
            return -1;
        }

        // Pre-allocate waiter before it's needed
        // Temporarily exit critical section to allocate
        irq_restore_local(flags);

        tty_waiter_t *waiter = (tty_waiter_t *)mm_malloc(sizeof(tty_waiter_t));
        if (waiter == NULL) {
            return -1;  // Memory allocation failed
        }

        // Re-enter critical section
        flags = irq_save_local();

        // Double-check conditions after re-entering
        if (t->size > 0 || t->eof) {
            // Data became available or EOF while we were allocating
            irq_restore_local(flags);
            mm_free(waiter);
            continue;  // Retry read
        }

        enqueue_waiter(t, current, waiter);
        current->state = BLOCKED;
        current->ticks_left = 0;

        irq_restore_local(flags);

        sched_force_yield();
    }

    return total;
}

// Escribe caracteres en pantalla (TTY es de salida inmediata)
int tty_write(tty_t *t, const void *buf, int n) {
    (void)t;
    if (buf == NULL || n <= 0) {
        return -1;
    }

    const char *src = (const char *)buf;
    for (int i = 0; i < n; i++) {
        vDriver_print(src[i], WHITE, BLACK);
    }
    return n;
}

// Cerrar la TTY no hace nada; mantiene compatibilidad con fd_ops
int tty_close(tty_t *t) {
    (void)t;
    return 0;
}

// Encola un nuevo carácter proveniente del teclado y despierta lectores
void tty_push_char(tty_t *t, char c) {
    if (t == NULL) {
        return;
    }

    uint64_t flags = irq_save_local();

    // Manejar Ctrl+C (ETX = 0x03)
    if (c == 3) {
        irq_restore_local(flags);
        
        // Obtener el proceso en foreground y matarlo
        const char *sigint_msg = "^C\n";
        tty_write(t, sigint_msg, 3);
        int fg_pid = proc_get_foreground_pid();
        if (fg_pid > 0) {
            proc_kill(fg_pid);
        }
        return;
    }

    if (c == 4) {
        t->eof = true;
        pcb_t *proc = dequeue_waiter(t);
        irq_restore_local(flags);
        if (proc != NULL) {
            proc_unblock(proc->pid);
        }
        return;
    }

    if (t->size == TTY_BUFFER_CAP) {
        t->head = (t->head + 1) % TTY_BUFFER_CAP;
        t->size--;
    }

    t->buffer[t->tail] = c;
    t->tail = (t->tail + 1) % TTY_BUFFER_CAP;
    t->size++;

    pcb_t *proc = dequeue_waiter(t);

    irq_restore_local(flags);

    if (proc != NULL) {
        proc_unblock(proc->pid);
    }
}

// Entrada principal desde el driver de teclado (solo encola si es printable)
void tty_handle_input(uint8_t scancode, char ascii) {
    (void)scancode;
    if (ascii == 0) {
        return;
    }

    tty_t *t = tty_default();
    if (t == NULL) {
        return;
    }

    tty_push_char(t, ascii);
}

// Establece el proceso que tiene control de la TTY (foreground)
void tty_set_foreground(int pid) {
    tty_t *t = tty_default();
    if (t == NULL) {
        return;
    }

    uint64_t flags = irq_save_local();
    t->fg_pid = pid;
    irq_restore_local(flags);
}

// Verifica si un proceso puede leer de la TTY
bool tty_can_read(int pid) {
    tty_t *t = tty_default();
    if (t == NULL) {
        return false;
    }

    uint64_t flags = irq_save_local();
    bool can_read = (t->fg_pid == pid);
    irq_restore_local(flags);

    return can_read;
}

// Obtiene el PID del proceso foreground actual
int tty_get_foreground(void) {
    tty_t *t = tty_default();
    if (t == NULL) {
        return -1;
    }

    uint64_t flags = irq_save_local();
    int fg = t->fg_pid;
    irq_restore_local(flags);

    return fg;
}
