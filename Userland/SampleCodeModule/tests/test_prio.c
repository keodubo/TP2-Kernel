#include <stdint.h>
#include <stdio.h>
#include "syscall.h"
#include "test_util.h"
#include "../include/sys_calls.h"

#define WORKER_COUNT 3

static volatile int running = 0;
static volatile uint64_t counters[WORKER_COUNT];

static void reset_counters(void) {
  for (int i = 0; i < WORKER_COUNT; i++) {
    counters[i] = 0;
  }
}

uint64_t priority_worker(uint64_t argc, char *argv[]) {
  int idx = 0;
  if (argc >= 1 && argv != NULL && argv[0] != NULL) {
    idx = satoi(argv[0]);
  }
  if (idx < 0 || idx >= WORKER_COUNT) {
    return (uint64_t)-1;
  }

  while (running) {
    counters[idx]++;
    if ((counters[idx] & 0xFF) == 0) {
      __asm__ __volatile__("pause");
    }
  }

  return counters[idx];
}

static void wait_processes(const int64_t pids[WORKER_COUNT], int exits[WORKER_COUNT]) {
  for (int i = 0; i < WORKER_COUNT; i++) {
    exits[i] = 0;
    if (pids[i] > 0) {
      my_wait_pid((int64_t)pids[i], &exits[i]);
    }
  }
}

static void run_phase(const char *label, const uint8_t prios[WORKER_COUNT], uint64_t duration_ms) {
  char *args0[] = {"0", NULL};
  char *args1[] = {"1", NULL};
  char *args2[] = {"2", NULL};
  char **worker_args[WORKER_COUNT] = {args0, args1, args2};

  int64_t pids[WORKER_COUNT] = {0};
  int exits[WORKER_COUNT] = {0};

  reset_counters();
  running = 1;

  for (int i = 0; i < WORKER_COUNT; i++) {
    pids[i] = my_create_process("priority_worker", 1, worker_args[i]);
    if (pids[i] < 0) {
      printf("test_priority: ERROR creating worker %d\n", i);
      running = 0;
      wait_processes(pids, exits);
      return;
    }
  }

  if (prios != NULL) {
    for (int i = 0; i < WORKER_COUNT; i++) {
      my_nice(pids[i], prios[i]);
    }
  }

  sys_wait(duration_ms);
  running = 0;
  sys_wait(50);

  wait_processes(pids, exits);

  printf("[test_priority] %s\n", label);
  for (int i = 0; i < WORKER_COUNT; i++) {
    printf("  worker %d -> counter=%llu exit=%d prio=%u\n",
           i, (unsigned long long)counters[i], exits[i],
           (prios != NULL) ? prios[i] : DEFAULT_PRIORITY);
  }
}

uint64_t test_priority(uint64_t argc, char *argv[]) {
  uint64_t duration = 1500;
  if (argc >= 1 && argv != NULL && argv[0] != NULL) {
    uint64_t parsed = satoi(argv[0]);
    if (parsed > 0) {
      duration = parsed;
    }
  }

  printf("\n[test_priority] Running with sample window %u ms\n", (unsigned int)duration);

  uint8_t same_prio[WORKER_COUNT] = {DEFAULT_PRIORITY, DEFAULT_PRIORITY, DEFAULT_PRIORITY};
  run_phase("Phase 1 - prioridades iguales", same_prio, duration);

  uint8_t mixed_prio[WORKER_COUNT] = {MAX_PRIORITY, DEFAULT_PRIORITY, MIN_PRIORITY};
  run_phase("Phase 2 - prioridades distintas (alto/medio/bajo)", mixed_prio, duration);

  return 0;
}
