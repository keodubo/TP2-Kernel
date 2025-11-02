#include <stdint.h>
#include <stdio.h>
#include "syscall.h"
#include "test_util.h"
#include "../include/sys_calls.h"

#define WAIT 10000000
#define TOTAL_PROCESSES 3

#define LOWEST 0
#define MEDIUM 1
#define HIGHEST 2

#define DEFAULT_LIMIT 1500

int64_t prio[TOTAL_PROCESSES] = {LOWEST, MEDIUM, HIGHEST};

// Convierte enteros (con signo) a string en buffers provistos
static void int_to_string(int value, char *buffer, size_t size) {
	if (size == 0) {
		return;
	}

	unsigned int magnitude = (value < 0) ? (unsigned int)(-value) : (unsigned int)value;
	char temp[12];
	int idx = 0;

	if (magnitude == 0) {
		temp[idx++] = '0';
	}

	while (magnitude > 0 && idx < (int)sizeof(temp)) {
		temp[idx++] = (char)('0' + (magnitude % 10));
		magnitude /= 10;
	}

	size_t pos = 0;
	if (value < 0 && pos + 1 < size) {
		buffer[pos++] = '-';
	}

	while (idx > 0 && pos + 1 < size) {
		buffer[pos++] = temp[--idx];
	}

	buffer[pos] = '\0';
}

static void uint64_to_string(uint64_t value, char *buffer, size_t size) {
	if (size == 0) {
		return;
	}

	char temp[32];
	int idx = 0;

	if (value == 0) {
		temp[idx++] = '0';
	}

	while (value > 0 && idx < (int)sizeof(temp)) {
		temp[idx++] = (char)('0' + (value % 10));
		value /= 10;
	}

	size_t pos = 0;
	while (idx > 0 && pos + 1 < size) {
		buffer[pos++] = temp[--idx];
	}

	buffer[pos] = '\0';
}

static void print_progress(const char *proc_id, uint64_t counter, uint64_t limit) {
	char counter_str[32];
	char limit_str[32];

	uint64_to_string(counter, counter_str, sizeof(counter_str));
	uint64_to_string(limit, limit_str, sizeof(limit_str));

	printf("[%s] %s/%s\n", proc_id, counter_str, limit_str);
}

// Proceso que incrementa un contador hasta alcanzar el límite indicado
void endless_pid(int argc, char **argv) {
	if (argc < 2) {
		printf("[test_prio] Invalid arguments for worker process\n");
		sys_exit(1);
	}

	const char *proc_id = argv[0];
		int64_t parsed_limit = satoi(argv[1]);
		uint64_t limit = parsed_limit > 0 ? (uint64_t)parsed_limit : DEFAULT_LIMIT;
		uint64_t counter = 0;
		uint64_t report_step = limit / 10;

		if (report_step == 0) {
			report_step = 1;
		}

		while (counter < limit) {
			counter++;

			if ((counter % report_step) == 0 || counter == limit) {
				print_progress(proc_id, counter, limit);
			}

			my_yield();
		}

		printf("[test_prio] Process %s reached target %s\n", proc_id, argv[1]);
		sys_exit(0);
	}

uint64_t test_prio(uint64_t argc, char *argv[]) {
	int64_t pids[TOTAL_PROCESSES];
	uint64_t i;

	uint64_t limit = DEFAULT_LIMIT;

	if (argc >= 1) {
		int64_t parsed = satoi(argv[0]);
		if (parsed > 0) {
			limit = (uint64_t)parsed;
		}
	}

	static char limit_str[32];
	uint64_to_string(limit, limit_str, sizeof(limit_str));

	printf("\n=== TEST PRIO START ===\n");
	printf("[test_prio] Target count per process: %s\n", limit_str);

	static char process_id_strs[TOTAL_PROCESSES][12];
	static char *process_args[TOTAL_PROCESSES][3];

	// Crear 3 procesos con misma prioridad inicial (MEDIUM)
	for (i = 0; i < TOTAL_PROCESSES; i++) {
		int_to_string(i, process_id_strs[i], sizeof(process_id_strs[i]));
		process_args[i][0] = process_id_strs[i];
		process_args[i][1] = limit_str;
		process_args[i][2] = NULL;

		pids[i] = sys_create_process(endless_pid, 2, process_args[i], "test_prio_worker", MEDIUM);
		printf("[test_prio] Created process %d, returned PID=%d\n", (int)i, (int)pids[i]);
	}

	printf("[test_prio] Starting PHASE 1 observation...\n");

	// FASE 1: Observar con misma prioridad
	bussy_wait(WAIT);
	printf("\nCHANGING PRIORITIES...\n");
	printf("NEW PROCESS PRIORITY:\n");
	for (i = 0; i < TOTAL_PROCESSES; i++) {
		printf("PROCESS: %d  PRIORITY: %d\n", (int)i, (int)prio[i]);
	}

	// FASE 2: Cambiar prioridades y observar
	for (i = 0; i < TOTAL_PROCESSES; i++) {
		my_nice(pids[i], prio[i]);
	}

	bussy_wait(WAIT);
	printf("\nBLOCKING...\n");

	// FASE 3: Bloquear procesos
	for (i = 0; i < TOTAL_PROCESSES; i++) {
		my_block(pids[i]);
	}

	printf("CHANGING PRIORITIES WHILE BLOCKED...\n");
	printf("NEW PROCESS PRIORITY:\n");
	for (i = 0; i < TOTAL_PROCESSES; i++) {
		printf("PROCESS: %d  PRIORITY: %d\n", (int)i, MEDIUM);
	}

	// Cambiar prioridades mientras están bloqueados
	for (i = 0; i < TOTAL_PROCESSES; i++) {
		my_nice(pids[i], MEDIUM);
	}

	printf("UNBLOCKING...\n");

	// Desbloquear procesos
	for (i = 0; i < TOTAL_PROCESSES; i++) {
		my_unblock(pids[i]);
	}

	// Observar con misma prioridad de nuevo
	bussy_wait(WAIT);

	printf("\nWAITING FOR PROCESSES TO FINISH...\n");
	for (i = 0; i < TOTAL_PROCESSES; i++) {
		int status = 0;
		if (my_wait_pid(pids[i], &status) < 0) {
			printf("[test_prio] Failed to wait process %d (PID=%d)\n", (int)i, (int)pids[i]);
		}
	}

	printf("[test_prio] All processes reached their targets.\n");
	return 0;
}
