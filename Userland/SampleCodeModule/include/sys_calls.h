#ifndef _SYSCALLS_H_
#define _SYSCALLS_H_

#include <stdint.h>
#include <colors.h>

// Estructura para informacion de memoria (debe coincidir con la del kernel)
typedef struct {
    uint64_t total_memory;
    uint64_t used_memory;
    uint64_t free_memory;
    uint64_t allocated_blocks;
    uint64_t free_blocks;
} memory_info_t;

/*
 * Pasaje de parametros en C:
   %rdi %rsi %rdx %rcx %r8 %r9
 */

uint64_t sys_scrWidth();

uint64_t sys_drawRectangle(int x, int y, int x2, int y2, Color color);

uint64_t sys_wait(uint64_t ms);

uint64_t sys_registerInfo(uint64_t reg[17]);

uint64_t sys_printmem(uint64_t mem);

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

// Process management syscalls
int64_t sys_getpid();

int64_t sys_create_process(void (*entry_point)(int, char**), int argc, char** argv, const char* name, uint8_t priority);

int64_t sys_kill(int pid);

int64_t sys_block(int pid);

int64_t sys_unblock(int pid);

int64_t sys_nice(int pid, uint8_t new_priority);

int64_t sys_yield();

int64_t sys_wait_pid(int pid);

#endif
