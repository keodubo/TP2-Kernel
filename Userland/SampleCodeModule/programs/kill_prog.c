#include <stdio.h>
#include <sys_calls.h>
#include <spawn_args.h>
#include <parse_utils.h>

// Comando kill: Termina un proceso especificado por su PID
// Uso: kill <pid>
void kill_main(int argc, char **argv) {
	// Validar que se reciba el PID como argumento
	if (argc < 2) {
		printf("\nUsage: kill <pid>\n");
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	// Parsear el PID del proceso a terminar
	int pid = 0;
	if (!parse_int_token(argv[1], &pid)) {
		printf("\nkill: invalid pid '%s'\n", argv[1]);
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	// Invocar syscall para terminar el proceso
	if (sys_kill(pid) < 0) {
		printf("\nkill: failed to terminate pid %d\n", pid);
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	printf("\nKilled pid %d\n", pid);
	free_spawn_args(argv, argc);
	sys_exit(0);
}
