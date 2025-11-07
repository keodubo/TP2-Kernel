#ifndef _SYSCALLS_H_
#define _SYSCALLS_H_

#include <stdint.h>
#include <colors.h>
#include <mm_stats.h>

// Wrapper de syscalls que el userland expone como librería estándar

// Estructura para informacion de memoria (debe coincidir con la del kernel)
typedef struct {
    uint64_t total_memory;
    uint64_t used_memory;
    uint64_t free_memory;
    uint64_t allocated_blocks;
    uint64_t free_blocks;
} memory_info_t;

typedef struct {
    int pid;
    int priority;
    int state;
    int ticks_left;
    int fg;
    char name[32];
    uint64_t sp;
    uint64_t bp;
} proc_info_t;

/*
 * Pasaje de parametros en C:
   %rdi %rsi %rdx %rcx %r8 %r9
 */

uint64_t sys_scrWidth();

uint64_t sys_drawRectangle(int x, int y, int x2, int y2, Color color);

uint64_t sys_wait(uint64_t ms);

uint64_t sys_registerInfo(uint64_t reg[17]);

uint64_t sys_printmem(uint64_t mem);

// Legacy syscalls (para compatibilidad - escriben UN SOLO CARÁCTER)
uint64_t sys_read(uint64_t fd, char *buf);

uint64_t sys_write(uint64_t fd, const char buf);

uint64_t sys_writeColor(uint64_t fd, char buffer, Color color);

uint64_t sys_clear();

uint64_t sys_getHours();

uint64_t sys_pixelPlus();

uint64_t sys_pixelMinus();

uint64_t sys_playSpeaker(uint32_t frequence, uint64_t duration);

uint64_t sys_stopSpeaker();

uint64_t sys_getMinutes();

uint64_t sys_getSeconds();

uint64_t sys_scrHeight();

uint64_t sys_drawCursor();

// Memory management syscalls
void* sys_malloc(uint64_t size);

uint64_t sys_free(void* ptr);

uint64_t sys_mem_info(memory_info_t* info);
int64_t sys_mm_get_stats(mm_stats_t *stats);

// Process management syscalls
int64_t sys_getpid();

int64_t sys_create_process(void (*entry_point)(int, char**), int argc, char** argv, const char* name, uint8_t priority);

// Nueva syscall para crear procesos con control de foreground/background
int64_t sys_create_process_ex(void (*entry_point)(int, char**), int argc, char** argv, const char* name, uint8_t priority, int is_fg);

int64_t sys_kill(int pid);

int64_t sys_block(int pid);

int64_t sys_unblock(int pid);

int64_t sys_nice(int pid, uint8_t new_priority);

int64_t sys_yield();

int64_t sys_wait_pid(int pid, int *status);

int64_t sys_wait_children(int *status);

int64_t sys_exit(int code);

int64_t sys_proc_snapshot(proc_info_t *buffer, uint64_t max_count);

int64_t sys_sem_open(const char *name, unsigned int init);
int64_t sys_sem_wait(int sem_id);
int64_t sys_sem_post(int sem_id);
int64_t sys_sem_close(int sem_id);
int64_t sys_sem_unlink(const char *name);

// Pipes (Hito 5)
int sys_pipe_open(const char *name, int flags);  // flags: 1=R, 2=W, 3=RW
int sys_pipe_close(int fd);
int sys_pipe_read(int fd, void *buf, int n);
int sys_pipe_write(int fd, const void *buf, int n);
int sys_pipe_unlink(const char *name);

// FD genéricos (pueden usar stdin=0, stdout=1, stderr=2)
int sys_read_fd(int fd, void *buf, int n);
int sys_write_fd(int fd, const void *buf, int n);
int sys_close_fd(int fd);
int sys_dup2(int oldfd, int newfd);

#define MIN_PRIORITY 0
#define MAX_PRIORITY 3
#define DEFAULT_PRIORITY 2
#define MAX_PROCS 128

#endif
