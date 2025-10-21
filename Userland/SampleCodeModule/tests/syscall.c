#include <stdint.h>
#include <stddef.h>
#include "syscall.h"
#include "../include/sys_calls.h"
#include "../include/userlib.h"

extern void endless_loop_wrapper(int, char**);
extern void endless_loop_print_wrapper(int, char**);
extern uint64_t my_process_inc(uint64_t argc, char *argv[]);
extern void zero_to_max();

static void my_process_inc_entry(int argc, char **argv) {
  (void)my_process_inc((uint64_t)argc, argv);
}

static void zero_to_max_entry(int argc, char **argv) {
  (void)argc;
  (void)argv;
  zero_to_max();
}

int64_t my_getpid() {
  return (int64_t)sys_getpid();
}

int64_t my_create_process(char *name, uint64_t argc, char *argv[]) {
  // Declarar las funciones wrapper
  
  void (*func)(int, char**) = endless_loop_wrapper;
  
  // Simple function name matching
  if (name != 0) {
    // Compare strings manually since we might not have strcmp
    char *el = "endless_loop";
    char *elp = "endless_loop_print";
    char *mpi = "my_process_inc";
    char *ztm = "zero_to_max";
    int i;
    
    // Check endless_loop
    for (i = 0; el[i] != '\0' && name[i] != '\0'; i++) {
      if (el[i] != name[i]) break;
    }
    if (el[i] == '\0' && name[i] == '\0') {
      func = endless_loop_wrapper;
    } else {
      // Check endless_loop_print
      for (i = 0; elp[i] != '\0' && name[i] != '\0'; i++) {
        if (elp[i] != name[i]) break;
      }
      if (elp[i] == '\0' && name[i] == '\0') {
        func = endless_loop_print_wrapper;
      } else {
        // Check my_process_inc
        for (i = 0; mpi[i] != '\0' && name[i] != '\0'; i++) {
          if (mpi[i] != name[i]) break;
        }
        if (mpi[i] == '\0' && name[i] == '\0') {
          func = my_process_inc_entry;
        } else {
          // Check zero_to_max
          for (i = 0; ztm[i] != '\0' && name[i] != '\0'; i++) {
            if (ztm[i] != name[i]) break;
          }
          if (ztm[i] == '\0' && name[i] == '\0') {
            func = zero_to_max_entry;
          }
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

typedef struct {
  char name[32];
  int handle;
  int in_use;
} sem_entry_t;

static sem_entry_t sem_entries[16] = {0};

static int find_sem_entry(const char *name) {
  for (int i = 0; i < 16; i++) {
    if (sem_entries[i].in_use && strcmp(sem_entries[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

static int store_sem_entry(const char *name, int handle) {
  int idx = find_sem_entry(name);
  if (idx >= 0) {
    sem_entries[idx].handle = handle;
    return idx;
  }
  for (int i = 0; i < 16; i++) {
    if (!sem_entries[i].in_use) {
      strcpy(sem_entries[i].name, name);
      sem_entries[i].handle = handle;
      sem_entries[i].in_use = 1;
      return i;
    }
  }
  return -1;
}

int64_t my_sem_open(char *sem_id, uint64_t initialValue) {
  if (sem_id == NULL) {
    return -1;
  }
  int handle = (int)sys_sem_open(sem_id, (unsigned int)initialValue);
  if (handle < 0) {
    return -1;
  }
  if (store_sem_entry(sem_id, handle) < 0) {
    sys_sem_close(handle);
    return -1;
  }
  return handle;
}

int64_t my_sem_wait(char *sem_id) {
  int idx = find_sem_entry(sem_id);
  if (idx < 0) {
    return -1;
  }
  return sys_sem_wait(sem_entries[idx].handle);
}

int64_t my_sem_post(char *sem_id) {
  int idx = find_sem_entry(sem_id);
  if (idx < 0) {
    return -1;
  }
  return sys_sem_post(sem_entries[idx].handle);
}

int64_t my_sem_close(char *sem_id) {
  int idx = find_sem_entry(sem_id);
  if (idx < 0) {
    return -1;
  }
  int result = sys_sem_close(sem_entries[idx].handle);
  if (result == 0) {
    sem_entries[idx].in_use = 0;
  }
  return result;
}

int64_t my_sem_unlink(char *sem_id) {
  int result = sys_sem_unlink(sem_id);
  if (result == 0) {
    int idx = find_sem_entry(sem_id);
    if (idx >= 0) {
      sem_entries[idx].in_use = 0;
    }
  }
  return result;
}

int64_t my_yield() {
  return (int64_t)sys_yield();
}

int64_t my_wait(int64_t pid) {
  return my_wait_pid(pid, NULL);
}

int64_t my_wait_pid(int64_t pid, int *status) {
  return (int64_t)sys_wait_pid((int)pid, status);
}
