#include <stddef.h>
#include "pipe.h"
#include "memory_manager.h"
#include "interrupts.h"
#include "lib.h"
#include "sched.h"

// Implementación de pipes nominales con bloqueo y wakeups explícitos

static kpipe_t *pipe_buckets[PIPE_HASH_BUCKETS] = {0};

// Helpers internos
static uint32_t pipe_hash(const char *name);          // Hash simple para tabla
static int pipe_name_cmp(const char *a, const char *b);
static void pipe_name_copy(char *dst, const char *src);
static uint64_t irq_save(void);                      // Helpers críticos
static void irq_restore(uint64_t flags);
static void pipe_free(kpipe_t *p);
static void enqueue_reader(kpipe_t *p, pcb_t *proc, pipe_waiter_t *w); // Manejo de waiters
static void enqueue_writer(kpipe_t *p, pcb_t *proc, pipe_waiter_t *w);
static pcb_t* dequeue_reader(kpipe_t *p);
static pcb_t* dequeue_writer(kpipe_t *p);

static uint32_t pipe_hash(const char *name) {
    uint32_t hash = 5381;
    char c;
    while (name != NULL && (c = *name++) != '\0') {
        hash = ((hash << 5) + hash) + (uint32_t)c;
    }
    return hash % PIPE_HASH_BUCKETS;
}

static int pipe_name_cmp(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return (int)(unsigned char)(*a) - (int)(unsigned char)(*b);
        }
        a++;
        b++;
    }
    return (int)(unsigned char)(*a) - (int)(unsigned char)(*b);
}

// Implementacion de pipes nominales con bloqueo y wakeups explicitos

static void pipe_name_copy(char *dst, const char *src) {
    uint32_t i = 0;
    for (; i < PIPE_NAME_MAX - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile("pushfq\n\tpop %0" : "=r"(flags));
    _cli();
    return flags;
}

static void irq_restore(uint64_t flags) {
    if (flags & (1ULL << 9)) {
        _sti();
    }
}

static void pipe_free(kpipe_t *p) {
    if (p == NULL) return;
    
    // Liberar waiters de lectura
    pipe_waiter_t *w = p->r_head;
    while (w != NULL) {
        pipe_waiter_t *next = w->next;
        mm_free(w);
        w = next;
    }
    
    // Liberar waiters de escritura
    w = p->w_head;
    while (w != NULL) {
        pipe_waiter_t *next = w->next;
        mm_free(w);
        w = next;
    }
    
    mm_free(p);
}

static void enqueue_reader(kpipe_t *p, pcb_t *proc, pipe_waiter_t *w) {
    if (w == NULL) return;
    
    w->proc = proc;
    w->next = NULL;
    
    if (p->r_tail == NULL) {
        p->r_head = p->r_tail = w;
    } else {
        p->r_tail->next = w;
        p->r_tail = w;
    }
}

static void enqueue_writer(kpipe_t *p, pcb_t *proc, pipe_waiter_t *w) {
    if (w == NULL) return;
    
    w->proc = proc;
    w->next = NULL;
    
    if (p->w_tail == NULL) {
        p->w_head = p->w_tail = w;
    } else {
        p->w_tail->next = w;
        p->w_tail = w;
    }
}

static pcb_t* dequeue_reader(kpipe_t *p) {
    if (p->r_head == NULL) return NULL;
    
    pipe_waiter_t *w = p->r_head;
    p->r_head = w->next;
    if (p->r_head == NULL) {
        p->r_tail = NULL;
    }
    
    pcb_t *proc = w->proc;
    mm_free(w);
    return proc;
}

static pcb_t* dequeue_writer(kpipe_t *p) {
    if (p->w_head == NULL) return NULL;
    
    pipe_waiter_t *w = p->w_head;
    p->w_head = w->next;
    if (p->w_head == NULL) {
        p->w_tail = NULL;
    }
    
    pcb_t *proc = w->proc;
    mm_free(w);
    return proc;
}

// Obtiene (o crea) un pipe identificado por nombre y ajusta contadores de uso
int kpipe_open(const char* name, bool for_read, bool for_write, kpipe_t **out) {
    if (name == NULL || out == NULL) {
        return -1;
    }
    
    uint64_t flags = irq_save();
    
    // Buscar pipe existente
    uint32_t bucket = pipe_hash(name);
    kpipe_t *cursor = pipe_buckets[bucket];
    while (cursor != NULL) {
        if (!cursor->unlinked && pipe_name_cmp(cursor->name, name) == 0) {
            // Pipe encontrado, incrementar contadores
            if (for_read) cursor->readers++;
            if (for_write) cursor->writers++;
            irq_restore(flags);
            *out = cursor;
            return 0;
        }
        cursor = cursor->next_hash;
    }
    
    irq_restore(flags);
    
    // Crear nuevo pipe
    kpipe_t *new_pipe = (kpipe_t *)mm_malloc(sizeof(kpipe_t));
    if (new_pipe == NULL) {
        return -1;
    }
    
    memset(new_pipe, 0, sizeof(kpipe_t));
    pipe_name_copy(new_pipe->name, name);
    new_pipe->r = 0;
    new_pipe->w = 0;
    new_pipe->size = 0;
    new_pipe->readers = for_read ? 1 : 0;
    new_pipe->writers = for_write ? 1 : 0;
    new_pipe->unlinked = false;
    new_pipe->r_head = new_pipe->r_tail = NULL;
    new_pipe->w_head = new_pipe->w_tail = NULL;
    new_pipe->next_hash = NULL;
    
    // Insertar en hash table
    flags = irq_save();
    
    // Double-check por race condition
    cursor = pipe_buckets[bucket];
    while (cursor != NULL) {
        if (!cursor->unlinked && pipe_name_cmp(cursor->name, name) == 0) {
            if (for_read) cursor->readers++;
            if (for_write) cursor->writers++;
            irq_restore(flags);
            pipe_free(new_pipe);
            *out = cursor;
            return 0;
        }
        cursor = cursor->next_hash;
    }
    
    new_pipe->next_hash = pipe_buckets[bucket];
    pipe_buckets[bucket] = new_pipe;
    irq_restore(flags);
    
    *out = new_pipe;
    return 0;
}

// Cierra un extremo del pipe y despierta a quienes necesiten continuar
int kpipe_close(kpipe_t *p, bool was_read, bool was_write) {
    if (p == NULL) {
        return -1;
    }
    
    uint64_t flags = irq_save();
    
    if (was_read && p->readers > 0) {
        p->readers--;
    }
    if (was_write && p->writers > 0) {
        p->writers--;
    }
    
    // Si ya no hay writers, despertar todos los lectores (EOF)
    bool should_wake_readers = (p->writers == 0 && p->r_head != NULL);
    
    // Si ya no hay readers, despertar todos los escritores (EPIPE)
    bool should_wake_writers = (p->readers == 0 && p->w_head != NULL);
    
    bool should_free = (p->unlinked && p->readers == 0 && p->writers == 0);
    
    irq_restore(flags);
    
    // Despertar fuera de la sección crítica
    if (should_wake_readers) {
        flags = irq_save();
        pcb_t *proc;
        while ((proc = dequeue_reader(p)) != NULL) {
            irq_restore(flags);
            proc_unblock(proc->pid);
            flags = irq_save();
        }
        irq_restore(flags);
    }
    
    if (should_wake_writers) {
        flags = irq_save();
        pcb_t *proc;
        while ((proc = dequeue_writer(p)) != NULL) {
            irq_restore(flags);
            proc_unblock(proc->pid);
            flags = irq_save();
        }
        irq_restore(flags);
    }
    
    if (should_free) {
        pipe_free(p);
    }
    
    return 0;
}

// Lee del buffer circular; bloquea si no hay datos y aún existen escritores
int kpipe_read(kpipe_t *p, void *buf, int n) {
    if (p == NULL || buf == NULL || n <= 0) {
        return -1;
    }
    
    uint8_t *dest = (uint8_t *)buf;
    int total_read = 0;
    
    while (total_read < n) {
        uint64_t flags = irq_save();
        
        // Fast path: hay datos disponibles
        if (p->size > 0) {
            int remaining = n - total_read;
            int to_read = ((int)p->size < remaining) ? (int)p->size : remaining;
            
            // Copiar desde el ring buffer
            for (int i = 0; i < to_read; i++) {
                dest[total_read++] = p->buf[p->r];
                p->r = (p->r + 1) % PIPE_CAP;
                p->size--;
            }
            
            // Si hay escritores esperando, despertar uno
            bool should_wake_writer = (p->w_head != NULL);
            pcb_t *writer_proc = NULL;
            if (should_wake_writer) {
                writer_proc = dequeue_writer(p);
            }
            
            irq_restore(flags);
            
            // Despertar fuera de la SC
            if (writer_proc != NULL) {
                proc_unblock(writer_proc->pid);
            }
            
            // Si leímos algo, devolver lo leído (transferencia parcial permitida)
            if (total_read > 0) {
                return total_read;
            }
            continue;
        }
        
        // size == 0: verificar si hay writers o es EOF
        if (p->writers == 0) {
            // EOF: no hay más escritores
            irq_restore(flags);
            return total_read; // 0 si no leímos nada, >0 si leímos algo antes
        }
        
        // size == 0 && writers > 0: bloquear
        pcb_t *current = sched_current();
        if (current == NULL) {
            irq_restore(flags);
            return -1;
        }
        
        // Pre-allocate waiter BEFORE it's needed (still in critical section but before enqueue)
        // We need to temporarily exit critical section to allocate
        irq_restore(flags);

        pipe_waiter_t *waiter = (pipe_waiter_t *)mm_malloc(sizeof(pipe_waiter_t));
        if (waiter == NULL) {
            return -1;  // Memory allocation failed
        }

        // Re-enter critical section
        flags = irq_save();

        // Double-check conditions after re-entering (they might have changed)
        if (p->size > 0) {
            // Data became available while we were allocating
            irq_restore(flags);
            mm_free(waiter);
            continue;  // Retry read
        }

        if (p->writers == 0) {
            // Writers closed while we were allocating
            irq_restore(flags);
            mm_free(waiter);
            return total_read;
        }

        enqueue_reader(p, current, waiter);

        // *** FIX: Marcar como BLOQUEADO dentro de la sección crítica
        // (previene lost wakeup, igual que en semáforos)
        current->state = BLOCKED;
        current->ticks_left = 0;
        
        irq_restore(flags);
        
        // Ceder CPU sin busy-wait
        sched_force_yield();
        
        // Al despertar, reintentar el loop
    }
    
    return total_read;
}

// Escribe en el pipe; bloquea si el buffer está lleno y hay lectores activos
int kpipe_write(kpipe_t *p, const void *buf, int n) {
    if (p == NULL || buf == NULL || n <= 0) {
        return -1;
    }
    
    const uint8_t *src = (const uint8_t *)buf;
    int total_written = 0;
    
    while (total_written < n) {
        uint64_t flags = irq_save();
        
        // Verificar si hay lectores (EPIPE)
        if (p->readers == 0) {
            irq_restore(flags);
            return -1; // EPIPE: no hay lectores
        }
        
        // Fast path: hay espacio disponible
        if (p->size < PIPE_CAP) {
            int space = (int)(PIPE_CAP - p->size);
            int remaining = n - total_written;
            int to_write = (space < remaining) ? space : remaining;
            
            // Copiar al ring buffer
            for (int i = 0; i < to_write; i++) {
                p->buf[p->w] = src[total_written++];
                p->w = (p->w + 1) % PIPE_CAP;
                p->size++;
            }
            
            // Si hay lectores esperando, despertar uno
            bool should_wake_reader = (p->r_head != NULL);
            pcb_t *reader_proc = NULL;
            if (should_wake_reader) {
                reader_proc = dequeue_reader(p);
            }
            
            irq_restore(flags);
            
            // Despertar fuera de la SC
            if (reader_proc != NULL) {
                proc_unblock(reader_proc->pid);
            }
            
            // Si escribimos algo, devolver lo escrito (transferencia parcial permitida)
            if (total_written > 0) {
                return total_written;
            }
            continue;
        }
        
        // size == PIPE_CAP: pipe lleno, bloquear
        pcb_t *current = sched_current();
        if (current == NULL) {
            irq_restore(flags);
            return -1;
        }
        
        // Pre-allocate waiter BEFORE it's needed
        // Temporarily exit critical section to allocate
        irq_restore(flags);

        pipe_waiter_t *waiter = (pipe_waiter_t *)mm_malloc(sizeof(pipe_waiter_t));
        if (waiter == NULL) {
            return -1;  // Memory allocation failed
        }

        // Re-enter critical section
        flags = irq_save();

        // Double-check conditions after re-entering
        if (p->size < PIPE_CAP) {
            // Space became available while we were allocating
            irq_restore(flags);
            mm_free(waiter);
            continue;  // Retry write
        }

        enqueue_writer(p, current, waiter);

        // *** FIX: Marcar como BLOQUEADO dentro de la sección crítica
        current->state = BLOCKED;
        current->ticks_left = 0;
        
        irq_restore(flags);
        
        // Ceder CPU sin busy-wait
        sched_force_yield();
        
        // Al despertar, reintentar el loop
    }
    
    return total_written;
}

// Desvincula el nombre del pipe; se libera cuando no quedan referencias
int kpipe_unlink(const char* name) {
    if (name == NULL) {
        return -1;
    }
    
    uint64_t flags = irq_save();
    
    uint32_t bucket = pipe_hash(name);
    kpipe_t *prev = NULL;
    kpipe_t *cursor = pipe_buckets[bucket];
    
    while (cursor != NULL) {
        if (!cursor->unlinked && pipe_name_cmp(cursor->name, name) == 0) {
            break;
        }
        prev = cursor;
        cursor = cursor->next_hash;
    }
    
    if (cursor == NULL) {
        irq_restore(flags);
        return -1; // No encontrado
    }
    
    // Remover de la hash table
    if (prev == NULL) {
        pipe_buckets[bucket] = cursor->next_hash;
    } else {
        prev->next_hash = cursor->next_hash;
    }
    cursor->next_hash = NULL;
    cursor->unlinked = true;
    
    bool should_free = (cursor->readers == 0 && cursor->writers == 0);
    
    irq_restore(flags);
    
    if (should_free) {
        pipe_free(cursor);
    }
    
    return 0;
}
