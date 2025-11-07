#include <stdio.h>
#include <sys_calls.h>
#include <spawn_args.h>

static const char *state_to_string(int state) {
	switch (state) {
	case 0:
		return "NEW";
	case 1:
		return "READY";
	case 2:
		return "RUN";
	case 3:
		return "BLOCK";
	case 4:
		return "EXIT";
	default:
		return "?";
	}
}

static void format_hex64(uint64_t value, char out[17]) {
	static const char *digits = "0123456789ABCDEF";
	for (int i = 15; i >= 0; i--) {
		out[i] = digits[value & 0xF];
		value >>= 4;
	}
	out[16] = '\0';
}

static void print_hex64(uint64_t value) {
	char buf[17];
	format_hex64(value, buf);
	printf("0x%s", buf);
}

void ps_main(int argc, char **argv) {
	proc_info_t info[MAX_PROCS];
	int count = (int)sys_proc_snapshot(info, MAX_PROCS);
	
	if (count <= 0) {
		printf("\nNo processes to show\n");
		free_spawn_args(argv, argc);
		sys_exit(0);
	}

	printf("\nPID   PRIO STATE  TICKS FG        SP                BP                NAME\n");
	for (int i = 0; i < count; i++) {
		const char *state = state_to_string(info[i].state);
		const char *fg = info[i].fg ? "FG" : "BG";
		printf("%-5d %-4d %-6s %-5d %-2s ",
			   info[i].pid,
			   info[i].priority,
			   state,
			   info[i].ticks_left,
			   fg);
		print_hex64(info[i].sp);
		printf(" ");
		print_hex64(info[i].bp);
		// Ensure name is null-terminated and print it
		printf(" %-16s\n", info[i].name[0] ? info[i].name : "(no name)");
	}
	printf("\n");  // Extra newline for readability

	free_spawn_args(argv, argc);
	sys_exit(0);
}
