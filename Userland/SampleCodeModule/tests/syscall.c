#include <stdint.h>
#include <stddef.h>
#include "syscall.h"
#include "../include/sys_calls.h"
#include "../include/userlib.h"

extern void endless_loop_wrapper(int, char**);
extern void endless_loop_print_wrapper(int, char**);
extern uint64_t my_process_inc(uint64_t argc, char *argv[]);

static void my_process_inc_entry(int argc, char **argv) {
  (void)my_process_inc((uint64_t)argc, argv);
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
    char *elw = "endless_loop_wrapper";
    char *elp = "endless_loop_print";
    char *elp_w = "endless_loop_print_wrapper";
    char *mpi = "my_process_inc";
    int i;

    // Verificar endless_loop_wrapper primero (más específico)
    for (i = 0; elw[i] != '\0' && name[i] != '\0'; i++) {
      if (elw[i] != name[i]) break;
    }
    if (elw[i] == '\0' && name[i] == '\0') {
      func = endless_loop_wrapper;
    } else {
      // Verificar endless_loop
      for (i = 0; el[i] != '\0' && name[i] != '\0'; i++) {
        if (el[i] != name[i]) break;
      }
      if (el[i] == '\0' && name[i] == '\0') {
        func = endless_loop_wrapper;
      } else {
        // Verificar endless_loop_print_wrapper
        for (i = 0; elp_w[i] != '\0' && name[i] != '\0'; i++) {
          if (elp_w[i] != name[i]) break;
        }
        if (elp_w[i] == '\0' && name[i] == '\0') {
          func = endless_loop_print_wrapper;
        } else {
          // Verificar endless_loop_print
          for (i = 0; elp[i] != '\0' && name[i] != '\0'; i++) {
            if (elp[i] != name[i]) break;
          }
          if (elp[i] == '\0' && name[i] == '\0') {
            func = endless_loop_print_wrapper;
          } else {
            // Verificar my_process_inc
            for (i = 0; mpi[i] != '\0' && name[i] != '\0'; i++) {
              if (mpi[i] != name[i]) break;
            }
            if (mpi[i] == '\0' && name[i] == '\0') {
              func = my_process_inc_entry;
            }
          }
        }
      }
    }
  }
  
  // Los procesos hijos NO deben ser foreground (0 en vez de 1)
  // Solo el proceso que corre directamente desde la shell debe ser fg
  return (int64_t)sys_create_process(func, (int)argc, argv, name, 0);
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
  int owner_pid;
  int in_use;
} sem_entry_t;

static sem_entry_t sem_entries[16] = {0};

static int find_sem_entry(const char *name, int owner_pid) {
  for (int i = 0; i < 16; i++) {
    if (sem_entries[i].in_use &&
        sem_entries[i].owner_pid == owner_pid &&
        strcmp(sem_entries[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

static int store_sem_entry(const char *name, int owner_pid, int handle) {
  int idx = find_sem_entry(name, owner_pid);
  if (idx >= 0) {
    sem_entries[idx].handle = handle;
    return idx;
  }
  for (int i = 0; i < 16; i++) {
    if (!sem_entries[i].in_use) {
      strcpy(sem_entries[i].name, name);
      sem_entries[i].handle = handle;
      sem_entries[i].owner_pid = owner_pid;
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
  int owner_pid = (int)my_getpid();
  if (store_sem_entry(sem_id, owner_pid, handle) < 0) {
    sys_sem_close(handle);
    return -1;
  }
  return handle;
}

int64_t my_sem_wait(char *sem_id) {
  int owner_pid = (int)my_getpid();
  int idx = find_sem_entry(sem_id, owner_pid);
  if (idx < 0) {
    return -1;
  }
  return sys_sem_wait(sem_entries[idx].handle);
}

int64_t my_sem_post(char *sem_id) {
  int owner_pid = (int)my_getpid();
  int idx = find_sem_entry(sem_id, owner_pid);
  if (idx < 0) {
    return -1;
  }
  return sys_sem_post(sem_entries[idx].handle);
}

int64_t my_sem_close(char *sem_id) {
  int owner_pid = (int)my_getpid();
  int idx = find_sem_entry(sem_id, owner_pid);
  if (idx < 0) {
    return -1;
  }
  int result = sys_sem_close(sem_entries[idx].handle);
  if (result == 0) {
    sem_entries[idx].in_use = 0;
    sem_entries[idx].owner_pid = 0;
    sem_entries[idx].handle = 0;
    sem_entries[idx].name[0] = '\0';
  }
  return result;
}

int64_t my_sem_unlink(char *sem_id) {
  int result = sys_sem_unlink(sem_id);
  if (result == 0) {
    int owner_pid = (int)my_getpid();
    int idx = find_sem_entry(sem_id, owner_pid);
    if (idx >= 0) {
      sem_entries[idx].in_use = 0;
      sem_entries[idx].owner_pid = 0;
      sem_entries[idx].handle = 0;
      sem_entries[idx].name[0] = '\0';
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
