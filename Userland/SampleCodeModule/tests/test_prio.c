#include <stdint.h>
#include <stdio.h>
#include "syscall.h"
#include "test_util.h"
#include "../include/sys_calls.h"

#define MINOR_WAIT "500000000"
#define WAIT 1000000000
#define TOTAL_PROCESSES 3

// Prioridades MENORES o IGUALES que test_priority (que tiene prioridad 2)
// NUNCA usar prioridad 3 porque es mayor que test_priority y causa deadlock
#define LOWEST 0   
#define MEDIUM 1   
#define HIGHEST 2  

// File descriptors
#define DEV_NULL -1
#define STDIN 0
#define STDOUT 1
#define STDERR 2

int64_t prio[TOTAL_PROCESSES] = {LOWEST, MEDIUM, HIGHEST};

// Helper 1: Para observar con todos en el mismo nivel (Fases 1 y 3)
// El test se baja a MEDIUM para competir equitativamente
#define QUANTUM_MS 50  // Quantum aproximado del scheduler
static inline void observe_equal(uint64_t ms) {
    int64_t self = my_getpid();
    my_nice(self, MEDIUM);              // Mismo nivel que los hijos
    uint64_t loops = (ms / QUANTUM_MS) + 10;
    for (uint64_t i = 0; i < loops; i++) {
        my_yield();                     // Round-robin con los hijos
    }
    my_nice(self, HIGHEST);             // Restaurar
}

// Helper 2: Para observar con prioridades diferentes (Fase 2)
// El test se mantiene en HIGHEST pero hace yields frecuentes
static inline void observe_prio_diff(uint64_t ms) {
    int64_t self = my_getpid();
    my_nice(self, HIGHEST);             // Mantener prioridad alta
    uint64_t loops = (ms / QUANTUM_MS) * 4;  // Muchos más yields
    for (uint64_t i = 0; i < loops; i++) {
        my_yield();                     // Ceder CPU frecuentemente
    }
}

static char *intToChar(int i) {
	static char buffer[12];
	int j = 0;

	if (i < 0) {
		buffer[j++] = '-';
		i			= -i;
	}

	if (i == 0) {
		buffer[j++] = '0';
	}

	while (i > 0) {
		buffer[j++] = (i % 10) + '0';
		i /= 10;
	}

	buffer[j] = '\0';

	for (int k = 0; k < j / 2; k++) {
		char temp		  = buffer[k];
		buffer[k]		  = buffer[j - k - 1];
		buffer[j - k - 1] = temp;
	}

	return buffer;
}

void endless_pid(int argc, char **argv) {
	(void)argc;
	
	if (argc < 2 || argv[1] == NULL) {
		return;
	}
	
	while (1) {
		// Busy wait MUY corto para mejor intercalación
		for (volatile int i = 0; i < 50000; i++)
			;
		
		// Escribir directamente a STDOUT siempre con espacio
		// El terminal hará el wrapping natural
		char buf[3];
		buf[0] = argv[1][0];
		buf[1] = ' ';  // Siempre espacio
		buf[2] = '\0';
		sys_write_fd(STDOUT, buf, 2);
		
		// Yield para dar oportunidad a otros procesos
		my_yield();
	}
}

uint64_t test_prio(uint64_t argc, char *argv[]) {
  int64_t pids[TOTAL_PROCESSES];
  uint64_t i;
  // Buffers separados para cada proceso
  static char arg_buffers[TOTAL_PROCESSES][12];
  static char *args_arrays[TOTAL_PROCESSES][3];

  // Parsear parámetro de duración (en ms)
  uint64_t duration_ms = 2000;  // Default
  if (argc > 0 && argv[0] != NULL) {
    duration_ms = satoi(argv[0]);
    if (duration_ms == 0) {
      duration_ms = 2000;  // Valor por defecto si no es válido
    }
  }
  
  printf("\n=== TEST PRIORITY ===\n");
  printf("Duration per phase: %lu ms\n", duration_ms);
  
  // Preparar argumentos para cada proceso
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    // Copiar el número a un buffer único para cada proceso
    char *src = intToChar(i);
    int j = 0;
    while (src[j] != '\0') {
      arg_buffers[i][j] = src[j];
      j++;
    }
    arg_buffers[i][j] = '\0';
    
    args_arrays[i][0] = "endless_loop";
    args_arrays[i][1] = arg_buffers[i];
    args_arrays[i][2] = NULL;
  }
  
  // Crear 3 procesos con prioridad MEDIA para que tengan tiempo de CPU inicial
  printf("Creating %d processes with priority MEDIUM...\n", TOTAL_PROCESSES);
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    pids[i] = sys_create_process(endless_pid, 2, args_arrays[i], "endless_loop", MEDIUM);
    if (pids[i] < 0) {
      printf("ERROR: Failed to create process %ld\n", i);
      return -1;
    }
    printf("Created process %ld (PID: %ld)\n", i, pids[i]);
  }

  printf("\nFase 1 - Equal distribution (MEDIUM priority): \n");
  observe_equal(duration_ms);  // Fase 1: Test compite equitativamente
  printf("\n\n");
  
  printf("CHANGING PRIORITIES to different levels...\n");
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    printf("  Process %ld (PID %ld) -> Priority %ld\n", i, pids[i], prio[i]);
    my_nice(pids[i], prio[i]);
  }

  printf("\nFase 2 - Priority scheduling (0=LOW, 1=MED, 2=HIGH): \n");
  observe_prio_diff(duration_ms);  // Fase 2: Test cede CPU pero mantiene control
  printf("\n\n");
  
  printf("BLOCKING all processes...\n");
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    my_block(pids[i]);
  }

  printf("All processes blocked. Changing priorities while blocked...\n");
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    printf("  Process %ld (PID %ld) -> Priority MEDIUM\n", i, pids[i]);
    my_nice(pids[i], MEDIUM);
  }

  printf("\nUNBLOCKING all processes...\n");
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    my_unblock(pids[i]);
  }

  printf("Fase 3 - Equal distribution again (after reset): \n");
  observe_equal(duration_ms);  // Fase 3: Test compite equitativamente otra vez
  printf("\n\n");
  
  printf("KILLING all processes...\n");
  for (i = 0; i < TOTAL_PROCESSES; i++) {
    int64_t result = my_kill(pids[i]);
    if (result == 0) {
      printf("  Killed process %ld (PID %ld)\n", i, pids[i]);
    } else {
      printf("  ERROR: Failed to kill process %ld (PID %ld)\n", i, pids[i]);
    }
  }

  // Pequeña espera para asegurar que los procesos terminen limpiamente
  bussy_wait(100000000);

  printf("\n=== TEST COMPLETED ===\n\n");
  return 0;
}