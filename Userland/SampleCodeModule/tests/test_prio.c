#include <stdint.h>
#include <stdio.h>
#include "syscall.h"
#include "test_util.h"

#define TOTAL_PROCESSES 3

// Priority mapping for TP2 kernel (from Kernel/include/sched.h):
// MIN_PRIO=0, HIGHEST_PRIO=3 (MAX_PRIOS-1), DEFAULT_PRIO=2
// Lower numeric value = lower priority, higher numeric = higher priority
#define LOWEST   0  // Lowest priority
#define MEDIUM   2  // Medium priority (default)
#define HIGHEST  3  // Highest priority

int64_t prio[TOTAL_PROCESSES] = {LOWEST, MEDIUM, HIGHEST};

// Recommended max_value >= 50000000 to observe priority effects clearly.
// Adjust if quantum/ticks are very short or processes finish simultaneously.
uint64_t max_value = 0;

void zero_to_max() {
  uint64_t value = 0;

  while (value++ != max_value);

  printf("PROCESS %ld DONE!\n", (long)my_getpid());
}

uint64_t test_prio(uint64_t argc, char *argv[]) {
  int64_t pids[TOTAL_PROCESSES];
  char *ztm_argv[] = {0};
  uint64_t i;

  // Validate arguments: argc must be 1 (max_value parameter)
  if (argc != 1)
    return -1;

  if ((max_value = satoi(argv[0])) <= 0)
    return -1;

  printf("SAME PRIORITY...\n");

  for (i = 0; i < TOTAL_PROCESSES; i++) {
    pids[i] = my_create_process("zero_to_max", 0, ztm_argv);
    if (pids[i] < 0) {
      printf("ERROR: failed to create process\n");
      return -1;
    }
  }

  // Expect to see them finish at the same time

  for (i = 0; i < TOTAL_PROCESSES; i++)
    my_wait(pids[i]);

  printf("SAME PRIORITY, THEN CHANGE IT...\n");

  for (i = 0; i < TOTAL_PROCESSES; i++) {
    pids[i] = my_create_process("zero_to_max", 0, ztm_argv);
    if (pids[i] < 0) {
      printf("ERROR: failed to create process\n");
      return -1;
    }
    my_nice(pids[i], prio[i]);
    printf("  PROCESS %ld NEW PRIORITY: %ld\n", (long)pids[i], (long)prio[i]);
  }

  // Expect the priorities to take effect

  for (i = 0; i < TOTAL_PROCESSES; i++)
    my_wait(pids[i]);

  printf("SAME PRIORITY, THEN CHANGE IT WHILE BLOCKED...\n");

  for (i = 0; i < TOTAL_PROCESSES; i++) {
    pids[i] = my_create_process("zero_to_max", 0, ztm_argv);
    if (pids[i] < 0) {
      printf("ERROR: failed to create process\n");
      return -1;
    }
    my_block(pids[i]);
    my_nice(pids[i], prio[i]);
    printf("  PROCESS %ld NEW PRIORITY: %ld\n", (long)pids[i], (long)prio[i]);
  }

  for (i = 0; i < TOTAL_PROCESSES; i++)
    my_unblock(pids[i]);

  // Expect the priorities to take effect

  for (i = 0; i < TOTAL_PROCESSES; i++)
    my_wait(pids[i]);

  return 0;
}