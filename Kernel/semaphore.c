#include "include/semaphore.h"
#include "include/memory_manager.h"
#include "include/interrupts.h"
#include "include/lib.h"

#define SEM_HANDLE_MAX 128

static ksem_t *sem_buckets[KSEM_HASH_BUCKETS] = {0};

static uint32_t sem_hash(const char *name);
static uint32_t sem_name_len(const char *name);
static int sem_name_cmp(const char *a, const char *b);
static void sem_name_copy(char *dst, const char *src);
static uint64_t irq_save(void);
static void irq_restore(uint64_t flags);
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

static void sem_free(ksem_t *sem) {
    if (sem == NULL) {
        return;
    }
    sem_waiter_t *waiter = sem->wait_head;
    while (waiter != NULL) {
        sem_waiter_t *next = waiter->next;
        mm_free(waiter);
        waiter = next;
    }
    mm_free(sem);
}

int ksem_open(const char *name, unsigned int init, ksem_t **out) {
    if (name == NULL || out == NULL) {
        return -1;
    }

    uint32_t len = sem_name_len(name);
    if (len == 0 || len >= KSEM_NAME_MAX) {
        return -1;
    }

    ksem_t *sem = NULL;
    uint64_t flags = irq_save();

    uint32_t bucket = sem_hash(name);
    ksem_t *cursor = sem_buckets[bucket];
    while (cursor != NULL) {
        if (!cursor->unlinked && sem_name_cmp(cursor->name, name) == 0) {
            cursor->refcount++;
            sem = cursor;
            break;
        }
        cursor = cursor->hash_next;
    }

    irq_restore(flags);

    if (sem != NULL) {
        *out = sem;
        return 0;
    }

    ksem_t *new_sem = (ksem_t *)mm_malloc(sizeof(ksem_t));
    if (new_sem == NULL) {
        return -1;
    }
    memset(new_sem, 0, sizeof(ksem_t));
    sem_name_copy(new_sem->name, name);
    new_sem->count = init;
    new_sem->refcount = 1;
    new_sem->unlinked = false;
    new_sem->wait_head = NULL;
    new_sem->wait_tail = NULL;
    new_sem->hash_next = NULL;

    flags = irq_save();

    cursor = sem_buckets[bucket];
    while (cursor != NULL) {
        if (!cursor->unlinked && sem_name_cmp(cursor->name, name) == 0) {
            cursor->refcount++;
            irq_restore(flags);
            sem_free(new_sem);
            *out = cursor;
            return 0;
        }
        cursor = cursor->hash_next;
    }

    new_sem->hash_next = sem_buckets[bucket];
    sem_buckets[bucket] = new_sem;
    irq_restore(flags);

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

    uint64_t flags = irq_save();
    if (sem->count > 0) {
        sem->count--;
        irq_restore(flags);
        return 0;
    }

    // count == 0: encolar y BLOQUEAR sin ventana de carrera
    sem_waiter_t *waiter = (sem_waiter_t *)mm_malloc(sizeof(sem_waiter_t));
    if (waiter == NULL) {
        irq_restore(flags);
        return -1;
    }
    waiter->proc = current;
    waiter->next = NULL;

    if (sem->wait_tail == NULL) {
        sem->wait_head = waiter;
        sem->wait_tail = waiter;
    } else {
        sem->wait_tail->next = waiter;
        sem->wait_tail = waiter;
    }

    // *** FIX: marcar el proceso como BLOQUEADO dentro de la sección crítica
    // Esto previene el "lost wakeup": si otro CPU hace ksem_post() ahora,
    // el proceso ya está marcado como BLOCKED y proc_unblock() funcionará correctamente
    current->state = BLOCKED;
    current->ticks_left = 0;

    irq_restore(flags);

    // Ceder CPU inmediatamente sin busy-wait
    // (sched_force_yield usa el mismo camino que el timer interrupt)
    sched_force_yield();
    return 0;
}

int ksem_post(ksem_t *sem) {
    if (sem == NULL) {
        return -1;
    }

    sem_waiter_t *waiter = NULL;
    uint64_t flags = irq_save();
    if (sem->wait_head != NULL) {
        waiter = sem->wait_head;
        sem->wait_head = waiter->next;
        if (sem->wait_head == NULL) {
            sem->wait_tail = NULL;
        }
    } else {
        if (sem->count < UINT32_MAX) {
            sem->count++;
        }
    }
    irq_restore(flags);

    if (waiter != NULL) {
        proc_unblock(waiter->proc->pid);
        mm_free(waiter);
    }

    return 0;
}

int ksem_close(ksem_t *sem) {
    if (sem == NULL) {
        return -1;
    }

    uint64_t flags = irq_save();
    if (sem->refcount == 0) {
        irq_restore(flags);
        return -1;
    }
    sem->refcount--;
    bool should_free = (sem->unlinked && sem->refcount == 0 && sem->wait_head == NULL);
    irq_restore(flags);

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
    cursor->unlinked = true;
    bool should_free = (cursor->refcount == 0 && cursor->wait_head == NULL);

    irq_restore(flags);

    if (should_free) {
        sem_free(cursor);
    }
    return 0;
}
