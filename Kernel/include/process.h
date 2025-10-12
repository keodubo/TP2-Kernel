#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>

// Estados posibles de un proceso
typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} process_state_t;

// Prioridades 0 es maxima mayor valor es menor prioridad
#define MIN_PRIORITY 0
#define MAX_PRIORITY 4
#define DEFAULT_PRIORITY 2

// Tamano del stack por proceso 64KB
#define PROCESS_STACK_SIZE (64 * 1024)

typedef struct {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t rip;
    uint64_t rflags;
} process_context_t;

typedef struct process_t {
    int pid;
    char name[64];
    process_state_t state;
    uint8_t priority;

    process_context_t context;
    void* stack_base;
    void* stack_top;

    void (*entry_point)(int, char**);
    int argc;
    char** argv;

    int parent_pid;
    int is_foreground;
    uint64_t quantum_count;
    int waiting_children;
    int exit_status;

    struct process_t* next;
    struct process_t* prev;
    struct process_t* queue_next;
    struct process_t* cleanup_next;
} process_t;

void process_init();
int process_create(void (*entry_point)(int, char**), int argc, char** argv,
                   const char* name, uint8_t priority, int foreground);
process_t* process_get_current();
process_t* process_get_by_pid(int pid);
int process_kill(int pid);
int process_set_priority(int pid, uint8_t new_priority);
int process_block(int pid);
int process_unblock(int pid);
void process_yield();
int process_count();
process_t** process_get_all(int* count);
void process_wait_children();
void process_exit_current();
void process_start();
void process_collect_zombies();

#endif