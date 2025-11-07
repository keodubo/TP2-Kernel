#include "semaphore.h"
#include "memory_manager.h"
#include "interrupts.h"
#include "lib.h"

/**
 * Implementación de Semáforos Nominales (Named Semaphores)
 *
 * Este módulo provee semáforos estilo POSIX que pueden ser compartidos entre
 * procesos no relacionados mediante nombres (strings).
 *
 * Características principales:
 * - Semáforos nominales: accesibles por nombre desde cualquier proceso
 * - Sincronización sin busy-wait: los procesos se bloquean realmente
 * - Cola FIFO: los procesos despiertan en orden de llegada
 * - Conteo de referencias: semáforos se destruyen automáticamente cuando no hay usuarios
 * - Thread-safe: usa spinlocks para proteger estructuras críticas
 *
 * Estructura de datos:
 * - Hash table: para búsqueda rápida por nombre (KSEM_HASH_BUCKETS buckets)
 * - Wait queue: cola FIFO de procesos esperando en cada semáforo
 * - Referencia counting: cada proceso que abre un semáforo incrementa refcount
 *
 * Operaciones principales:
 * - ksem_open(): Abre/crea semáforo por nombre con valor inicial
 * - ksem_wait(): Decrementa contador o bloquea si es 0 (operación P/down)
 * - ksem_post(): Incrementa contador o despierta un proceso (operación V/up)
 * - ksem_close(): Cierra handle y decrementa refcount
 * - ksem_unlink(): Marca para destrucción cuando refcount llegue a 0
 */

#define SEM_HANDLE_MAX 128  // Máximo de handles de semáforos por proceso

// Tabla hash de semáforos: array de listas enlazadas
static ksem_t *sem_buckets[KSEM_HASH_BUCKETS] = {0};

// Spinlock global para proteger la creación de nuevos semáforos
// (evita que dos procesos creen el mismo semáforo simultáneamente)
static volatile int sem_creation_lock = 0;

// Helpers de tabla hash y nombres
static uint32_t sem_hash(const char *name);
static uint32_t sem_name_len(const char *name);
static int sem_name_cmp(const char *a, const char *b);
static void sem_name_copy(char *dst, const char *src);

// Helpers de interrupciones locales
static uint64_t irq_save(void);
static void irq_restore(uint64_t flags);

// Helpers de cola FIFO de waiters
static void wait_queue_init(wait_queue_t *q);
static bool wait_queue_is_empty(const wait_queue_t *q);
static void wait_queue_push(wait_queue_t *q, sem_waiter_t *node);
static sem_waiter_t *wait_queue_pop(wait_queue_t *q);
static sem_waiter_t *wait_queue_remove_proc(wait_queue_t *q, pcb_t *proc);

// Helpers varios
static bool sem_ready_to_destroy_locked(ksem_t *sem);
static void sem_free(ksem_t *sem);

static uint32_t sem_hash(const char *name) {
    uint32_t hash = 5381;
    char c;
    while (name != NULL && (c = *name++) != '\0') {
        hash = ((hash << 5) + hash) + (uint32_t)c;
    }
    return hash % KSEM_HASH_BUCKETS;
}

static uint32_t sem_name_len(const char *name) {
    if (name == NULL) {
        return 0;
    }
    uint32_t len = 0;
    while (name[len] != '\0' && len < KSEM_NAME_MAX) {
        len++;
    }
    return len;
}

static int sem_name_cmp(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return (int)(unsigned char)(*a) - (int)(unsigned char)(*b);
        }
        a++;
        b++;
    }
    return (int)(unsigned char)(*a) - (int)(unsigned char)(*b);
}

static void sem_name_copy(char *dst, const char *src) {
    uint32_t i = 0;
    for (; i < KSEM_NAME_MAX - 1 && src[i] != '\0'; i++) {
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

static void wait_queue_init(wait_queue_t *q) {
    if (q != NULL) {
        q->head = NULL;
        q->tail = NULL;
    }
}

static bool wait_queue_is_empty(const wait_queue_t *q) {
    return q == NULL || q->head == NULL;
}

static void wait_queue_push(wait_queue_t *q, sem_waiter_t *node) {
    if (q == NULL || node == NULL) {
        return;
    }
    node->next = NULL;
    if (q->tail == NULL) {
        q->head = node;
        q->tail = node;
    } else {
        q->tail->next = node;
        q->tail = node;
    }
}

static sem_waiter_t *wait_queue_pop(wait_queue_t *q) {
    if (q == NULL || q->head == NULL) {
        return NULL;
    }
    sem_waiter_t *node = q->head;
    q->head = node->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    node->next = NULL;
    return node;
}

static sem_waiter_t *wait_queue_remove_proc(wait_queue_t *q, pcb_t *proc) {
    if (q == NULL || proc == NULL) {
        return NULL;
    }

    sem_waiter_t *removed_head = NULL;
    sem_waiter_t *removed_tail = NULL;

    sem_waiter_t *prev = NULL;
    sem_waiter_t *cursor = q->head;

    while (cursor != NULL) {
        sem_waiter_t *next = cursor->next;
        if (cursor->proc == proc) {
            if (prev == NULL) {
                q->head = next;
            } else {
                prev->next = next;
            }

            if (q->tail == cursor) {
                q->tail = prev;
            }

            cursor->next = NULL;
            if (removed_tail == NULL) {
                removed_head = cursor;
                removed_tail = cursor;
            } else {
                removed_tail->next = cursor;
                removed_tail = cursor;
            }
            cursor = next;
            continue;
        }
        prev = cursor;
        cursor = next;
    }

    return removed_head;
}

static bool sem_ready_to_destroy_locked(ksem_t *sem) {
    if (sem == NULL) {
        return false;
    }
    if (sem->destroying) {
        return false;
    }
    if (sem->unlinked && sem->refcount == 0 && wait_queue_is_empty(&sem->waiters)) {
        sem->destroying = true;
        return true;
    }
    return false;
}

static void sem_free(ksem_t *sem) {
    if (sem == NULL) {
        return;
    }
    sem_waiter_t *waiter;
    while ((waiter = wait_queue_pop(&sem->waiters)) != NULL) {
        mm_free(waiter);
    }
    mm_free(sem);
}

// -----------------------------------------------------------------------------
// API pública
// -----------------------------------------------------------------------------

int ksem_open(const char *name, unsigned int init, ksem_t **out) {
    if (name == NULL || out == NULL) {
        return -1;
    }

    uint32_t len = sem_name_len(name);
    if (len == 0 || len >= KSEM_NAME_MAX) {
        return -1;
    }

    while (__sync_lock_test_and_set(&sem_creation_lock, 1)) {
        __asm__ volatile("pause");
    }

    ksem_t *sem = NULL;
    uint64_t flags = irq_save();

    uint32_t bucket = sem_hash(name);
    ksem_t *cursor = sem_buckets[bucket];
    while (cursor != NULL) {
        if (!cursor->unlinked && sem_name_cmp(cursor->name, name) == 0) {
            spinlock_lock(&cursor->lock);
            cursor->refcount++;
            spinlock_unlock(&cursor->lock);
            sem = cursor;
            break;
        }
        cursor = cursor->hash_next;
    }

    irq_restore(flags);

    if (sem != NULL) {
        __sync_lock_release(&sem_creation_lock);
        *out = sem;
        return 0;
    }

    ksem_t *new_sem = (ksem_t *)mm_malloc(sizeof(ksem_t));
    if (new_sem == NULL) {
        __sync_lock_release(&sem_creation_lock);
        return -1;
    }

    memset(new_sem, 0, sizeof(ksem_t));
    spinlock_init(&new_sem->lock);
    wait_queue_init(&new_sem->waiters);
    sem_name_copy(new_sem->name, name);
    new_sem->count = init;
    new_sem->refcount = 1;
    new_sem->unlinked = false;
    new_sem->destroying = false;
    new_sem->hash_next = NULL;

    flags = irq_save();

    cursor = sem_buckets[bucket];
    while (cursor != NULL) {
        if (!cursor->unlinked && sem_name_cmp(cursor->name, name) == 0) {
            spinlock_lock(&cursor->lock);
            cursor->refcount++;
            spinlock_unlock(&cursor->lock);
            irq_restore(flags);
            __sync_lock_release(&sem_creation_lock);
            sem_free(new_sem);
            *out = cursor;
            return 0;
        }
        cursor = cursor->hash_next;
    }

    new_sem->hash_next = sem_buckets[bucket];
    sem_buckets[bucket] = new_sem;

    irq_restore(flags);
    __sync_lock_release(&sem_creation_lock);

    *out = new_sem;
    return 0;
}

int ksem_wait(ksem_t *sem) {
    if (sem == NULL) {
        return -1;
    }

    pcb_t *current = sched_current();
    if (current == NULL) {
        return -1;
    }

    sem_waiter_t *waiter = (sem_waiter_t *)mm_malloc(sizeof(sem_waiter_t));
    if (waiter == NULL) {
        extern void ncPrintDec(uint64_t value);
        extern void ncPrint(const char *string);
        ncPrint("[KERNEL] sem_wait: mm_malloc FAILED for PID ");
        ncPrintDec(current->pid);
        ncPrint("\n");
        return -1;
    }

    uint64_t flags = spinlock_lock_irqsave(&sem->lock);

    if (sem->count > 0) {
        sem->count--;
        spinlock_unlock_irqrestore(&sem->lock, flags);
        mm_free(waiter);
        return 0;
    }

    waiter->proc = current;
    wait_queue_push(&sem->waiters, waiter);

    current->state = BLOCKED;
    current->ticks_left = 0;

    spinlock_unlock_irqrestore(&sem->lock, flags);

    sched_force_yield();
    return 0;
}

int ksem_post(ksem_t *sem) {
    if (sem == NULL) {
        return -1;
    }

    sem_waiter_t *waiter = NULL;
    pcb_t *target = NULL;

    uint64_t flags = spinlock_lock_irqsave(&sem->lock);

    if (!wait_queue_is_empty(&sem->waiters)) {
        waiter = wait_queue_pop(&sem->waiters);
        if (waiter != NULL) {
            target = waiter->proc;
        }
    } else {
        if (sem->count < UINT32_MAX) {
            sem->count++;
        }
    }

    spinlock_unlock_irqrestore(&sem->lock, flags);

    if (waiter != NULL) {
        bool wake_success = false;
        if (target != NULL) {
            wake_success = (proc_unblock(target->pid) == 0);
        }

        if (!wake_success) {
            uint64_t fix_flags = spinlock_lock_irqsave(&sem->lock);
            if (sem->count < UINT32_MAX) {
                sem->count++;
            }
            spinlock_unlock_irqrestore(&sem->lock, fix_flags);
        }

        mm_free(waiter);
    }

    return 0;
}

int ksem_close(ksem_t *sem) {
    if (sem == NULL) {
        return -1;
    }

    bool should_free = false;
    uint64_t flags = spinlock_lock_irqsave(&sem->lock);

    if (sem->refcount == 0) {
        spinlock_unlock_irqrestore(&sem->lock, flags);
        return -1;
    }

    sem->refcount--;
    should_free = sem_ready_to_destroy_locked(sem);

    spinlock_unlock_irqrestore(&sem->lock, flags);

    if (should_free) {
        sem_free(sem);
    }

    return 0;
}

int ksem_unlink(const char *name) {
    if (name == NULL) {
        return -1;
    }

    uint32_t len = sem_name_len(name);
    if (len == 0 || len >= KSEM_NAME_MAX) {
        return -1;
    }

    uint64_t flags = irq_save();

    uint32_t bucket = sem_hash(name);
    ksem_t *prev = NULL;
    ksem_t *cursor = sem_buckets[bucket];
    while (cursor != NULL) {
        if (!cursor->unlinked && sem_name_cmp(cursor->name, name) == 0) {
            break;
        }
        prev = cursor;
        cursor = cursor->hash_next;
    }

    if (cursor == NULL) {
        irq_restore(flags);
        return -1;
    }

    if (prev == NULL) {
        sem_buckets[bucket] = cursor->hash_next;
    } else {
        prev->hash_next = cursor->hash_next;
    }
    cursor->hash_next = NULL;

    irq_restore(flags);

    bool should_free = false;
    uint64_t lock_flags = spinlock_lock_irqsave(&cursor->lock);
    cursor->unlinked = true;
    should_free = sem_ready_to_destroy_locked(cursor);
    spinlock_unlock_irqrestore(&cursor->lock, lock_flags);

    if (should_free) {
        sem_free(cursor);
    }

    return 0;
}

void ksem_remove_waiters_for(pcb_t *proc) {
    if (proc == NULL) {
        return;
    }

    sem_waiter_t *garbage_head = NULL;
    sem_waiter_t *garbage_tail = NULL;

    uint64_t flags = irq_save();

    for (int bucket = 0; bucket < KSEM_HASH_BUCKETS; bucket++) {
        ksem_t *sem = sem_buckets[bucket];
        while (sem != NULL) {
            ksem_t *next_sem = sem->hash_next;

            uint64_t lock_flags = spinlock_lock_irqsave(&sem->lock);
            sem_waiter_t *removed = wait_queue_remove_proc(&sem->waiters, proc);
            spinlock_unlock_irqrestore(&sem->lock, lock_flags);

            while (removed != NULL) {
                sem_waiter_t *next = removed->next;
                removed->next = NULL;
                if (garbage_tail == NULL) {
                    garbage_head = removed;
                    garbage_tail = removed;
                } else {
                    garbage_tail->next = removed;
                    garbage_tail = removed;
                }
                removed = next;
            }

            sem = next_sem;
        }
    }

    irq_restore(flags);

    while (garbage_head != NULL) {
        sem_waiter_t *next = garbage_head->next;
        mm_free(garbage_head);
        garbage_head = next;
    }
}
