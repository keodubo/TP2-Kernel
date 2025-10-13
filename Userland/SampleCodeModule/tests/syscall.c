#include <stdint.h>
#include "../include/sys_calls.h"

int64_t my_getpid() {
  return (int64_t)sys_getpid();
}

int64_t my_create_process(char *name, uint64_t argc, char *argv[]) {
  // For now, we use endless_loop as default function
  // In a real implementation, you'd have a function registry
  extern void endless_loop();
  extern void endless_loop_print(uint64_t wait);
  extern uint64_t my_process_inc(uint64_t argc, char *argv[]);
  
  void (*func)(int, char**) = (void (*)(int, char**))endless_loop;
  
  // Simple function name matching
  if (name != 0) {
    // Compare strings manually since we might not have strcmp
    char *el = "endless_loop";
    char *elp = "endless_loop_print";
    char *mpi = "my_process_inc";
    int i;
    
    // Check endless_loop
    for (i = 0; el[i] != '\0' && name[i] != '\0'; i++) {
      if (el[i] != name[i]) break;
    }
    if (el[i] == '\0' && name[i] == '\0') {
      func = (void (*)(int, char**))endless_loop;
    } else {
      // Check endless_loop_print
      for (i = 0; elp[i] != '\0' && name[i] != '\0'; i++) {
        if (elp[i] != name[i]) break;
      }
      if (elp[i] == '\0' && name[i] == '\0') {
        func = (void (*)(int, char**))endless_loop_print;
      } else {
        // Check my_process_inc
        for (i = 0; mpi[i] != '\0' && name[i] != '\0'; i++) {
          if (mpi[i] != name[i]) break;
        }
        if (mpi[i] == '\0' && name[i] == '\0') {
          func = (void (*)(int, char**))my_process_inc;
        }
      }
    }
  }
  
  return (int64_t)sys_create_process(func, (int)argc, argv, name, 1);
}

int64_t my_nice(uint64_t pid, uint64_t newPrio) {
  return (int64_t)sys_nice((int)pid, (uint8_t)newPrio);
}

int64_t my_kill(uint64_t pid) {
  return (int64_t)sys_kill((int)pid);
}

int64_t my_block(uint64_t pid) {
  return (int64_t)sys_block((int)pid);
}

int64_t my_unblock(uint64_t pid) {
  return (int64_t)sys_unblock((int)pid);
}

int64_t my_sem_open(char *sem_id, uint64_t initialValue) {
  // Semaphores not implemented yet
  return 0;
}

int64_t my_sem_wait(char *sem_id) {
  // Semaphores not implemented yet
  return 0;
}

int64_t my_sem_post(char *sem_id) {
  // Semaphores not implemented yet
  return 0;
}

int64_t my_sem_close(char *sem_id) {
  // Semaphores not implemented yet
  return 0;
}

int64_t my_yield() {
  return (int64_t)sys_yield();
}

int64_t my_wait(int64_t pid) {
  return (int64_t)sys_wait((int)pid);
}
