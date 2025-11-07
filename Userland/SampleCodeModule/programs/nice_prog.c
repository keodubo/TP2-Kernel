#include <stdio.h>
#include <sys_calls.h>
#include <spawn_args.h>
#include <parse_utils.h>

void nice_main(int argc, char **argv) {
	if (argc < 3) {
		printf("\nUsage: nice <pid> <prio>\n");
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	int pid = 0;
	if (!parse_int_token(argv[1], &pid)) {
		printf("\nnice: invalid pid '%s'\n", argv[1]);
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	int prio = 0;
	if (!parse_int_token(argv[2], &prio)) {
		printf("\nnice: invalid priority '%s'\n", argv[2]);
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	if (prio < MIN_PRIORITY || prio > MAX_PRIORITY) {
		printf("\nnice: priority out of range (%d-%d)\n", MIN_PRIORITY, MAX_PRIORITY);
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	if (sys_nice(pid, (uint8_t)prio) < 0) {
		printf("\nnice: failed to set priority for pid %d\n", pid);
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	printf("\nSet pid %d priority to %d\n", pid, prio);
	free_spawn_args(argv, argc);
	sys_exit(0);
}
