#ifndef KERNEL_SPINLOCK_H
#define KERNEL_SPINLOCK_H

#include <stdint.h>
#include "interrupts.h"

typedef struct {
    volatile int value;
} spinlock_t;

static inline void spinlock_init(spinlock_t *lock) {
    if (lock != NULL) {
        lock->value = 0;
    }
}

static inline uint64_t spinlock_lock_irqsave(spinlock_t *lock) {
    uint64_t flags;
    __asm__ volatile("pushfq\n\tpop %0" : "=r"(flags));
    _cli();
    while (__sync_lock_test_and_set(&lock->value, 1)) {
        __asm__ volatile("pause");
    }
    return flags;
}

static inline void spinlock_unlock_irqrestore(spinlock_t *lock, uint64_t flags) {
    __sync_lock_release(&lock->value);
    if (flags & (1ULL << 9)) {
        _sti();
    }
}

static inline void spinlock_lock(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->value, 1)) {
        __asm__ volatile("pause");
    }
}

static inline void spinlock_unlock(spinlock_t *lock) {
    __sync_lock_release(&lock->value);
}

#endif /* KERNEL_SPINLOCK_H */
