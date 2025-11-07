#include <stdio.h>
#include <sys_calls.h>
#include <spawn_args.h>
#include <parse_utils.h>

static proc_info_t *find_proc(proc_info_t *procs, int count, int pid) {
	for (int i = 0; i < count; i++) {
		if (procs[i].pid == pid) {
			return &procs[i];
		}
	}
	return NULL;
}

void block_main(int argc, char **argv) {
	if (argc < 2) {
		printf("\nUsage: block <pid>\n");
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	int target_pid = 0;
	if (!parse_int_token(argv[1], &target_pid) || target_pid <= 0) {
		printf("\nblock: invalid pid '%s'\n", argv[1]);
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	int shell_pid = -1;
	if (argc >= 3 && argv[2] != NULL) {
		(void)parse_int_token(argv[2], &shell_pid);
	}

	if (target_pid == shell_pid) {
		printf("\nblock: refusing to block the current shell\n");
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	proc_info_t procs[MAX_PROCS];
	int count = (int)sys_proc_snapshot(procs, MAX_PROCS);
	if (count <= 0) {
		printf("\nblock: could not read process list\n");
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	proc_info_t *target = find_proc(procs, count, target_pid);
	if (target == NULL) {
		printf("\nblock: pid %d not found\n", target_pid);
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	if (target->state == 4) {
		printf("\nblock: process already exited\n");
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

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

	if (target->state == 0) {
		printf("\nblock: process not started yet\n");
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	if (sys_block(target_pid) < 0) {
		printf("\nblock: failed to block process %d\n", target_pid);
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	printf("\nProcess %d moved to BLOCKED\n", target_pid);
	free_spawn_args(argv, argc);
	sys_exit(0);
}
