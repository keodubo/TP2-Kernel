#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

// Quantum en ticks de timer aprox 55 ms por tick con PIT default
#define QUANTUM_TICKS 1

void scheduler_init();
void scheduler_enable();
void scheduler_disable();
process_t* scheduler_tick();
process_t* scheduler_yield();
void scheduler_add_process(process_t* proc);
void scheduler_remove_process(process_t* proc);
process_t* scheduler_get_current();
process_t* scheduler_pick_next();
process_context_t* scheduler_handle_timer_tick(uint64_t* stack_ptr);
int scheduler_is_enabled();

#endif