#include <stdio.h>
#include <sys_calls.h>
#include <spawn_args.h>

static void print_help_text(void) {
	printf("\n\n===== Listing a preview of available commands =====\n");
	printf("\n>'help' or 'ls'     - displays this shell information");
	printf("\n>time               - display current time");
	printf("\n>clear              - clear the display");
	printf("\n>(+)                - increase font size (scaled)");
	printf("\n>(-)                - decrease font size (scaled)");
	printf("\n>registersinfo      - print current register values");
	printf("\n>zerodiv            - test divide by zero exception");
	printf("\n>invopcode          - test invalid op code exception");
	printf("\n>ps                 - list all processes");
	printf("\n>loop [-p prio]     - prints short greeting and process PID");
	printf("\n>nice <pid> <prio>  - change a given process priority");
	printf("\n>kill <pid>         - kill specified process");
	printf("\n>block <pid>        - toggle process between BLOCKED and READY");
	printf("\n>yield              - yield the CPU");
	printf("\n>waitpid <pid|-1>   - wait for a child to finish");
	printf("\n>echo <text>        - print text to stdout");
	printf("\n>mem [-v]           - show memory usage statistics");
	printf("\n>cat                - read from stdin and write to stdout");
	printf("\n>wc                 - count lines from stdin");
	printf("\n>filter             - remove vowels from stdin");
	printf("\n>test_mm [size]     - test memory manager (default: 100000000)");
	printf("\n>test_processes [n] - test process management (default: 10)");
	printf("\n>test_priority [n]  - scheduling demo (default: 5)");
	printf("\n>test_no_synchro [n]- run race condition without semaphores");
	printf("\n>test_synchro [n]   - run synchronized version using semaphores");
	printf("\n>mvar <writers> <readers> - start colored MVar demo");
	printf("\n>exit               - exit KERNEL OS\n\n");
	printf("Background execution:\n");
	printf("  cmd &  - Execute command in background (no wait)\n");
	printf("Pipe examples:\n");
	printf("  echo hola | wc         - count lines in 'hola'\n");
	printf("  echo \"hola mundo\" | wc  - count lines with spaces\n");
	printf("  echo abracadabra | filter - remove vowels\n");
	printf("  cat | filter           - read input and filter vowels\n\n");
}

void help_main(int argc, char **argv) {
	(void)argc;
	print_help_text();
	free_spawn_args(argv, argc);
	sys_exit(0);
}
