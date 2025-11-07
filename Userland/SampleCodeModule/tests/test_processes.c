#include <stdio.h>
#include "syscall.h"
#include "test_util.h"
#include "../include/userlib.h"
#include "../include/sys_calls.h"

enum State { RUNNING,
             BLOCKED,
             KILLED };

typedef struct P_rq {
  int32_t pid;
  enum State state;
} p_rq;

// Función helper para convertir estado a string
static const char *state_to_string(int state) {
  switch (state) {
  case 0:
    return "NEW";
  case 1:
    return "READY";
  case 2:
    return "RUN";
  case 3:
    return "BLOCK";
  case 4:
    return "EXIT";
  default:
    return "?";
  }
}

// Función para imprimir información de procesos como ps
static void print_processes_info(void) {
  proc_info_t info[128];  // MAX_PROCS = 128
  int count = sys_proc_snapshot(info, 128);
  
  if (count <= 0) {
    printf("\nNo processes to show\n");
    return;
  }

  // Imprimir encabezado
  printsColor("\nPID   PRIO STATE  TICKS FG        SP                BP                NAME", 254, GREEN);
  printf("\n");
  
  // Imprimir cada proceso
  for (int i = 0; i < count; i++) {
    const char *state = state_to_string(info[i].state);
    const char *fg = info[i].fg ? "FG" : "BG";
    printf("PID %d PRIO %d STATE %s TICKS %d %s ", info[i].pid, info[i].priority, state, info[i].ticks_left, fg);
    printf("0x");
    printHex(info[i].sp);
    printf(" ");
    printf("0x");
    printHex(info[i].bp);
    printf(" %s\n", info[i].name);
  }
}

int64_t test_processes(uint64_t argc, char *argv[]) {
  uint8_t rq;
  uint8_t alive = 0;
  uint8_t action;
  uint64_t max_processes;
  char *argvAux[] = {0};

  if (argc != 1)
    return -1;

  if ((max_processes = satoi(argv[0])) <= 0)
    return -1;

  p_rq p_rqs[max_processes];

  while (1) {

    // Crear max_processes procesos
    for (rq = 0; rq < max_processes; rq++) {
      p_rqs[rq].pid = my_create_process("endless_loop_wrapper", 0, argvAux);

      if (p_rqs[rq].pid == -1) {
        printf("test_processes: ERROR creating process\n");
        return -1;
      } else {
        p_rqs[rq].state = RUNNING;
        alive++;
      }
    }
    printf("\nProcesos creados. Estado actual:\n");
    print_processes_info();

    // Matar, bloquear o desbloquear procesos aleatoriamente hasta que todos hayan sido eliminados
    while (alive > 0) {

      for (rq = 0; rq < max_processes; rq++) {
        action = GetUniform(100) % 2;

        switch (action) {
          case 0:
            if (p_rqs[rq].state == RUNNING || p_rqs[rq].state == BLOCKED) {
              if (my_kill(p_rqs[rq].pid) == -1) {
                printf("test_processes: ERROR killing process\n");
                return -1;
              }
              p_rqs[rq].state = KILLED;
              alive--;
            }
            break;

          case 1:
            if (p_rqs[rq].state == RUNNING) {
              if (my_block(p_rqs[rq].pid) == -1) {
                printf("test_processes: ERROR blocking process\n");
                return -1;
              }
              p_rqs[rq].state = BLOCKED;
            }
            break;
        }
      }

      // Desbloquear procesos aleatoriamente
      for (rq = 0; rq < max_processes; rq++)
        if (p_rqs[rq].state == BLOCKED && GetUniform(100) % 2) {
          if (my_unblock(p_rqs[rq].pid) == -1) {
            printf("test_processes: ERROR unblocking process\n");
            return -1;
          }
          p_rqs[rq].state = RUNNING;
        }
      
      printf("\nEstado despues de modificaciones (vivos: %d):\n", alive);
      print_processes_info();
    }
  }
}