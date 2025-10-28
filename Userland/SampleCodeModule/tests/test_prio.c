#include <stdint.h>
#include <stdio.h>
#include "syscall.h"
#include "test_util.h"
#include "../include/sys_calls.h"
#include "../include/userlib.h"

#define TOTAL_PROCESSES 3

// Prioridades permitidas para los procesos hijos
#define LOWEST 0
#define MEDIUM 1
#define HIGHEST 2

static const char *priority_to_text(uint8_t prio) {
  switch (prio) {
    case LOWEST:
      return "LOW";
    case MEDIUM:
      return "MED";
    case HIGHEST:
      return "HIGH";
    default:
      return "?";
  }
}

static char *intToChar(int i) {
  static char buffer[12];
  int j = 0;

  if (i < 0) {
    buffer[j++] = '-';
    i = -i;
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
    char temp = buffer[k];
    buffer[k] = buffer[j - k - 1];
    buffer[j - k - 1] = temp;
  }

  return buffer;
}

static void u64_to_str(uint64_t value, char *buffer, uint32_t size) {
  if (size == 0) {
    return;
  }

  if (value == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return;
  }

  char temp[32];
  uint32_t idx = 0;

  while (value > 0 && idx < sizeof(temp)) {
    temp[idx++] = (char)('0' + (value % 10));
    value /= 10;
  }

  uint32_t out = 0;
  while (idx > 0 && out + 1 < size) {
    buffer[out++] = temp[--idx];
  }
  buffer[out] = '\0';
}

static void copy_string(char *dest, const char *src) {
  uint32_t i = 0;
  while (src[i] != '\0') {
    dest[i] = src[i];
    i++;
  }
  dest[i] = '\0';
}

static const uint8_t phase2_priorities[TOTAL_PROCESSES] = {LOWEST, MEDIUM, HIGHEST};
static const uint8_t phase1_priorities[TOTAL_PROCESSES] = {MEDIUM, MEDIUM, MEDIUM};

typedef enum {
  PHASE_EQUAL_PRIORITY,
  PHASE_CHANGE_RUNNING,
  PHASE_CHANGE_BLOCKED
} phase_mode_t;

void priority_counter_process(int argc, char **argv) {
  if (argc < 4) {
    sys_exit(-1);
    return;
  }

  int logical_id = 0;
  uint64_t limit = 0;
  const char *phase_label = argv[3] ? argv[3] : "Fase";

  if (argv[1] != NULL) {
    int64_t parsed_id = satoi(argv[1]);
    if (parsed_id >= 0) {
      logical_id = (int)parsed_id;
    }
  }
  if (argv[2] != NULL) {
    int64_t parsed_limit = satoi(argv[2]);
    if (parsed_limit > 0) {
      limit = (uint64_t)parsed_limit;
    }
  }

  if (limit == 0) {
    limit = 1;
  }

  int pid = (int)my_getpid();
  for (uint64_t counter = 0; counter < limit; counter++) {
    // Busy-wait para consumir tiempo de CPU y evidenciar la planificación
    for (volatile uint64_t spin = 0; spin < 20000; spin++)
      ;
  }

  printf("[%s] Proceso logico %d (PID %d) terminado\n", phase_label, logical_id, pid);
  sys_exit(0);
}

static int run_phase(const char *phase_title,
                     const char *phase_label,
                     const uint8_t priorities[TOTAL_PROCESSES],
                     uint64_t limit,
                     phase_mode_t mode,
                     char *args_arrays[TOTAL_PROCESSES][4],
                     char id_buffers[TOTAL_PROCESSES][12],
                     char limit_buffers[TOTAL_PROCESSES][32]) {
  int64_t pids[TOTAL_PROCESSES];

  printf("\n%s\n", phase_title);
  for (uint64_t i = 0; i < TOTAL_PROCESSES; i++) {
    const char *id_str = intToChar((int)i);
    copy_string(id_buffers[i], id_str);
    u64_to_str(limit, limit_buffers[i], sizeof(limit_buffers[i]));

    args_arrays[i][0] = "priority_counter";
    args_arrays[i][1] = id_buffers[i];
    args_arrays[i][2] = limit_buffers[i];
    args_arrays[i][3] = (char *)phase_label;

    uint8_t creation_priority = MEDIUM;
    if (mode == PHASE_EQUAL_PRIORITY) {
      creation_priority = priorities[i];
    }

    pids[i] = sys_create_process_ex(priority_counter_process, 4, args_arrays[i],
                                    "priority_counter", creation_priority, 0);
    if (pids[i] < 0) {
      printf("ERROR: no se pudo crear el proceso %d\n", (int)i);
      for (uint64_t j = 0; j < i; j++) {
        my_kill(pids[j]);
      }
      return -1;
    }

    if (mode == PHASE_CHANGE_BLOCKED) {
      my_block(pids[i]);
    }

    if (mode == PHASE_CHANGE_RUNNING || mode == PHASE_CHANGE_BLOCKED) {
      my_nice(pids[i], priorities[i]);
      printf("  Proceso %d (PID %d) nueva prioridad: %s\n",
             (int)i, (int)pids[i], priority_to_text(priorities[i]));
    } else {
      printf("  Proceso %d (PID %d) prioridad inicial: %s\n",
             (int)i, (int)pids[i], priority_to_text(creation_priority));
    }
  }

  if (mode == PHASE_CHANGE_BLOCKED) {
    printf("  Desbloqueando procesos...\n");
    for (uint64_t i = 0; i < TOTAL_PROCESSES; i++) {
      my_unblock(pids[i]);
    }
  }

  printf("  Esperando finalizacion de procesos...\n");
  for (uint64_t i = 0; i < TOTAL_PROCESSES; i++) {
    int status = 0;
    int64_t waited = my_wait_pid(pids[i], &status);
    if (waited < 0) {
      printf("    ERROR: waitpid fallo para PID %d\n", (int)pids[i]);
    } else {
      printf("    PID %d finalizo con estado %d\n", (int)waited, status);
    }
  }
  return 0;
}

uint64_t test_prio(uint64_t argc, char *argv[]) {
  static char id_buffers[TOTAL_PROCESSES][12];
  static char limit_buffers[TOTAL_PROCESSES][32];
  static char *args_arrays[TOTAL_PROCESSES][4];

  uint64_t count_limit = 1000;
  if (argc > 0 && argv[0] != NULL) {
    uint64_t parsed = (uint64_t)satoi(argv[0]);
    if (parsed > 0) {
      count_limit = parsed;
    }
  }

  printf("\n=== TEST PRIORITY ===\n");
  printf("Los procesos incrementaran hasta: %d\n", (int)count_limit);

  const char *phase_titles[] = {
      "Fase 1 - Prioridades iguales (todos MED)",
      "Fase 2 - Cambiar prioridades en ejecucion (LOW, MED, HIGH)",
      "Fase 3 - Cambiar prioridades mientras estan bloqueados"};
  const char *phase_labels[] = {"Fase 1", "Fase 2", "Fase 3"};

  if (run_phase(phase_titles[0], phase_labels[0], phase1_priorities,
                count_limit, PHASE_EQUAL_PRIORITY, args_arrays, id_buffers,
                limit_buffers) < 0) {
    return -1;
  }

  if (run_phase(phase_titles[1], phase_labels[1], phase2_priorities,
                count_limit, PHASE_CHANGE_RUNNING, args_arrays, id_buffers,
                limit_buffers) < 0) {
    return -1;
  }

  if (run_phase(phase_titles[2], phase_labels[2], phase2_priorities,
                count_limit, PHASE_CHANGE_BLOCKED, args_arrays, id_buffers,
                limit_buffers) < 0) {
    return -1;
  }

  printf("\n=== TEST COMPLETED ===\n\n");
  return 0;
}
