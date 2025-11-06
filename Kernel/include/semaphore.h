#ifndef KERNEL_SEMAPHORE_H
#define KERNEL_SEMAPHORE_H

#include <stdbool.h>
#include <stdint.h>
#include "sched.h"
#include "spinlock.h"

// Parámetros globales del sistema de semáforos nombrados

#define KSEM_NAME_MAX     32
#define KSEM_HASH_BUCKETS 32
#define KSEM_HANDLE_MAX   128

// Cola de procesos bloqueados sobre el semáforo
typedef struct sem_waiter {
    pcb_t *proc;
    struct sem_waiter *next;
} sem_waiter_t;

typedef struct wait_queue {
    sem_waiter_t *head;
    sem_waiter_t *tail;
} wait_queue_t;

// Semáforo kernel-space compartido entre procesos
typedef struct ksem {
    spinlock_t lock;
    char name[KSEM_NAME_MAX];
    unsigned int count;
    unsigned int refcount;
    bool unlinked;
    bool destroying;
    wait_queue_t waiters;
    struct ksem *hash_next;
} ksem_t;

int ksem_open(const char *name, unsigned int init, ksem_t **out);
int ksem_wait(ksem_t *sem);
int ksem_post(ksem_t *sem);
int ksem_close(ksem_t *sem);
int ksem_unlink(const char *name);
void ksem_remove_waiters_for(pcb_t *proc);

#endif
