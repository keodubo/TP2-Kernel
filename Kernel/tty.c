#include <stddef.h>
#include "include/tty.h"
#include "include/memory_manager.h"
#include "include/interrupts.h"
#include "include/lib.h"
#include "include/sched.h"
#include "videoDriver.h"

// Backend de la TTY principal: buffer circular + colas de procesos bloqueados

#define TTY_BUFFER_CAP 256

// Archivo: tty.c
// Propósito: Implementación de la TTY principal del sistema
// Resumen: Buffer circular, colas de procesos bloqueados por lectura y
//          API para lectura/escritura de la consola por procesos.

// Nodo de la cola de espera para lectores del teclado
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

static void enqueue_waiter(tty_t *t, pcb_t *proc) {
    tty_waiter_t *node = (tty_waiter_t *)mm_malloc(sizeof(tty_waiter_t));
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

        enqueue_waiter(t, current);
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
        int fg_pid = proc_get_foreground_pid();
        
        // DEBUG: Imprimir información
        char debug_msg[64];
        const char *sigint_msg = "^C\n";
        tty_write(t, sigint_msg, 3);
        
        if (fg_pid > 0) {
            // Formatear mensaje de debug
            int len = 0;
            const char *prefix = "[DEBUG] Killing FG process: ";
            for (int i = 0; prefix[i] != '\0'; i++) {
                debug_msg[len++] = prefix[i];
            }
            // Convertir PID a string simple
            if (fg_pid >= 10) {
                debug_msg[len++] = '0' + (fg_pid / 10);
            }
            debug_msg[len++] = '0' + (fg_pid % 10);
            debug_msg[len++] = '\n';
            tty_write(t, debug_msg, len);
            
            proc_kill(fg_pid);
        } else {
            const char *no_fg_msg = "[DEBUG] No FG process to kill\n";
            tty_write(t, no_fg_msg, 29);
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
