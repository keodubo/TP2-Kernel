#include <stdio.h>
#include <sys_calls.h>
#include <spawn_args.h>
#include <parse_utils.h>

// Busca un proceso por su PID en un arreglo de proc_info_t
// Retorna un puntero al proceso encontrado o NULL si no existe
static proc_info_t *find_proc(proc_info_t *procs, int count, int pid) {
	for (int i = 0; i < count; i++) {
		if (procs[i].pid == pid) {
			return &procs[i];
		}
	}
	return NULL;
}

// Comando block: Alterna el estado de un proceso entre BLOCKED y READY
// Si el proceso está bloqueado lo desbloquea, si está ready/running lo bloquea
void block_main(int argc, char **argv) {
	// Validar que se reciba el PID como argumento
	if (argc < 2) {
		printf("\nUsage: block <pid>\n");
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	// Parsear el PID del proceso a bloquear/desbloquear
	int target_pid = 0;
	if (!parse_int_token(argv[1], &target_pid) || target_pid <= 0) {
		printf("\nblock: invalid pid '%s'\n", argv[1]);
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	// Si se recibió PID de la shell, evitar bloquearla
	int shell_pid = -1;
	if (argc >= 3 && argv[2] != NULL) {
		(void)parse_int_token(argv[2], &shell_pid);
	}

	if (target_pid == shell_pid) {
		printf("\nblock: refusing to block the current shell\n");
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	// Obtener snapshot de todos los procesos del sistema
	proc_info_t procs[MAX_PROCS];
	int count = (int)sys_proc_snapshot(procs, MAX_PROCS);
	if (count <= 0) {
		printf("\nblock: could not read process list\n");
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	// Buscar el proceso objetivo en la lista
	proc_info_t *target = find_proc(procs, count, target_pid);
	if (target == NULL) {
		printf("\nblock: pid %d not found\n", target_pid);
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	// No se puede bloquear un proceso que ya terminó
	if (target->state == 4) {
		printf("\nblock: process already exited\n");
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	// Si está bloqueado (estado 3), desbloquearlo
	if (target->state == 3) {
		if (sys_unblock(target_pid) < 0) {
			printf("\nblock: failed to unblock process %d\n", target_pid);
			free_spawn_args(argv, argc);
			sys_exit(1);
		}
		printf("\nProcess %d moved to READY\n", target_pid);
		free_spawn_args(argv, argc);
		sys_exit(0);
	}

	// No se puede bloquear un proceso que no empezó
	if (target->state == 0) {
		printf("\nblock: process not started yet\n");
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	// Si está en otro estado (READY o RUNNING), bloquearlo
	if (sys_block(target_pid) < 0) {
		printf("\nblock: failed to block process %d\n", target_pid);
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	printf("\nProcess %d moved to BLOCKED\n", target_pid);
	free_spawn_args(argv, argc);
	sys_exit(0);
}
