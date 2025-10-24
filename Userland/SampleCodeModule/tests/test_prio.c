#include <stdint.h>
#include <stdio.h>
#include "syscall.h"
#include "test_util.h"

#define WAIT 50000000  // Muy corto para evitar deadlock (0.05 segundos)
#define TOTAL_PROCESSES 3

// Prioridades MENORES o IGUALES que test_priority (que tiene prioridad 2)
// NUNCA usar prioridad 3 porque es mayor que test_priority y causa deadlock
#define LOWEST 0   
#define MEDIUM 1   
#define HIGHEST 2  

int64_t prio[TOTAL_PROCESSES] = {LOWEST, MEDIUM, HIGHEST};

// Función similar a endless_pid del código de referencia
// Imprime el PID y hace yield después de un delay
void test_prio_endless(int argc, char **argv) {
  (void)argc;
  (void)argv;
  int64_t pid = my_getpid();
  
  while (1) {
    printf("%ld ", pid);
    // Delay activo (como el código de referencia)
    for (int i = 0; i < 50000000; i++)
      ;
    // IMPORTANTE: yield después de cada impresión
    my_yield();
  }
}

uint64_t test_prio(uint64_t argc, char *argv[]) {
  int64_t pids[TOTAL_PROCESSES];
  char *argv_empty[] = {0};
  uint64_t i;

  (void)argc;
  (void)argv;

  printf("\n=== TEST PRIORITY ===\n");
  
  // CRÍTICO: Subir la prioridad del test a la MÁS ALTA (3)
  // para evitar starvation cuando los hijos tengan prioridad 2
  my_nice(my_getpid(), 3);
  
  // Crear 3 procesos con prioridad por defecto
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    pids[i] = my_create_process("endless_loop_print", 0, argv_empty);
    if (pids[i] < 0) {
      printf("ERROR: Failed to create process\n");
      return -1;
    }
  }

  bussy_wait(WAIT);
  
  printf("\nCHANGING PRIORITIES...\n");
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    printf("  Process %ld -> Priority %ld\n", pids[i], prio[i]);
    my_nice(pids[i], prio[i]);
  }

  bussy_wait(WAIT);
  
  printf("\nBLOCKING...\n");
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    my_block(pids[i]);
  }

  printf("CHANGING PRIORITIES WHILE BLOCKED...\n");
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    printf("  Process %ld -> Priority %d\n", pids[i], MEDIUM);
    my_nice(pids[i], MEDIUM);
  }

  printf("UNBLOCKING...\n");
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    my_unblock(pids[i]);
  }

  bussy_wait(WAIT);
  
  printf("\nKILLING...\n");
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    my_kill(pids[i]);
  }

  // Restaurar la prioridad del test a DEFAULT
  my_nice(my_getpid(), 2);

  printf("\n=== TEST COMPLETED ===\n\n");
  return 0;
}