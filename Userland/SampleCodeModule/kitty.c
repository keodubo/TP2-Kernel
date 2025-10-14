#include <userlib.h>
#include <stdio.h>
#include <time.h>
#include <stddef.h>
#include <sys_calls.h>
#include <colors.h>
#include <eliminator.h>
#include <kitty.h>
#include <ascii.h>
#include "../tests/test_util.h"
#include "../tests/syscall.h"

// Declaraciones de las funciones de test
uint64_t test_mm(uint64_t argc, char *argv[]);
uint64_t test_processes(uint64_t argc, char *argv[]);
uint64_t test_sync(uint64_t argc, char *argv[]);
uint64_t test_no_synchro(uint64_t argc, char *argv[]);
uint64_t test_synchro(uint64_t argc, char *argv[]);

// Declaraciones de los comandos de pipes
int cat_main(int argc, char **argv);
int wc_main(int argc, char **argv);
int filter_main(int argc, char **argv);

static void copy_arg_or_default(char *dst, size_t dst_len, char **argv, int index, const char *fallback);
static void free_spawn_args(char **argv, int argc);
static int64_t spawn_test_process(const char *name, void (*entry)(int, char **), int argc, char **argv);
static void debug_log(const char *tag, const char *msg);
static void debug_log_u64(const char *tag, const char *label, uint64_t value);
static void debug_dump_args(const char *tag, int argc, char **argv);

static int debug_enabled = 0;
#define DBG_MSG(msg) debug_log("kitty", msg)
#define DBG_VAL(label, value) debug_log_u64("kitty", label, value)
#define DBG_ARGS(argc, argv) debug_dump_args("kitty", argc, argv)

static void copy_arg_or_default(char *dst, size_t dst_len, char **argv, int index, const char *fallback) {
	if (dst == NULL || dst_len == 0) {
		return;
	}
	if (argv != NULL && index >= 0 && argv[index] != NULL) {
		char *src = argv[index];
		size_t i;
		for (i = 0; i < dst_len - 1 && src[i] != '\0'; i++) {
			dst[i] = src[i];
		}
		dst[i] = '\0';
		return;
	}
	size_t i;
	for (i = 0; i < dst_len - 1 && fallback != NULL && fallback[i] != '\0'; i++) {
		dst[i] = fallback[i];
	}
	dst[i] = '\0';
}

static void free_spawn_args(char **argv, int argc) {
	if (argv == NULL) {
		return;
	}
	for (int i = 0; i < argc; i++) {
		if (argv[i] != NULL) {
			sys_free(argv[i]);
		}
	}
	sys_free(argv);
}

static int64_t spawn_test_process(const char *name, void (*entry)(int, char **), int argc, char **argv) {
    DBG_MSG("spawn_test_process");
    DBG_VAL("argc", (uint64_t)argc);
    int64_t pid = sys_create_process(entry, argc, argv, name, DEFAULT_PRIORITY);
    if (pid < 0 && argv != NULL) {
        free_spawn_args(argv, argc);
    }
    DBG_VAL("spawned pid", (uint64_t)pid);
    return pid;
}

static void debug_log(const char *tag, const char *msg) {
	if (!debug_enabled || tag == NULL || msg == NULL) {
		return;
	}
	printf("[DBG][%s] %s\n", tag, msg);
}

static void debug_log_u64(const char *tag, const char *label, uint64_t value) {
	if (!debug_enabled || tag == NULL || label == NULL) {
		return;
	}
	printf("[DBG][%s] %s: %u\n", tag, label, (unsigned int)value);
}

static void debug_dump_args(const char *tag, int argc, char **argv) {
	if (!debug_enabled || tag == NULL) {
		return;
	}
	printf("[DBG][%s] argc=%d\n", tag, argc);
	if (argv == NULL) {
		printf("[DBG][%s] argv=(null)\n", tag);
		return;
	}
	for (int i = 0; i < argc; i++) {
		printf("[DBG][%s] argv[%d]=%s\n", tag, i, argv[i] ? argv[i] : "(null)");
	}
}

// Wrappers para ejecutar tests como procesos
void test_mm_process(int argc, char **argv) {
	char arg_buffer[32];
	DBG_ARGS(argc, argv);
	DBG_MSG("test_mm_process start");
	
	printf("\n[test_mm_process] Starting with argc=%d\n", argc);
	copy_arg_or_default(arg_buffer, sizeof(arg_buffer), argv, 0, "100000000");
	printf("[test_mm_process] Using arg: %s\n", arg_buffer);
	free_spawn_args(argv, argc);

	char *args[2] = {arg_buffer, NULL};
	printf("[test_mm_process] Calling test_mm...\n");
	uint64_t result = test_mm(1, args);
	printf("[test_mm_process] Finished with result: %d\n", (int)result);
	sys_exit((int)result);
}

void test_processes_process(int argc, char **argv) {
	char arg_buffer[32];
	DBG_ARGS(argc, argv);
	DBG_MSG("test_processes_process start");
	
	printf("\n[test_processes_process] Starting with argc=%d\n", argc);
	copy_arg_or_default(arg_buffer, sizeof(arg_buffer), argv, 0, "10");
	printf("[test_processes_process] Using arg: %s\n", arg_buffer);
	free_spawn_args(argv, argc);

	char *args[2] = {arg_buffer, NULL};
	printf("[test_processes_process] Calling test_processes...\n");
	test_processes(1, args);
	printf("[test_processes_process] Finished\n");
	sys_exit(0);
}

void test_sync_process(int argc, char **argv) {
	char arg0_buffer[32];
	char arg1_buffer[32];
	DBG_ARGS(argc, argv);
	DBG_MSG("test_sync_process start");
	
	printf("\n[test_sync_process] Starting with argc=%d\n", argc);
	copy_arg_or_default(arg0_buffer, sizeof(arg0_buffer), argv, 0, "10");
	copy_arg_or_default(arg1_buffer, sizeof(arg1_buffer), argv, 1, "1");
	printf("[test_sync_process] Using args: %s, %s\n", arg0_buffer, arg1_buffer);
	free_spawn_args(argv, argc);

	char *args[3] = {arg0_buffer, arg1_buffer, NULL};
	printf("[test_sync_process] Calling test_sync...\n");
	test_sync(2, args);
	printf("[test_sync_process] Finished\n");
	sys_exit(0);
}

void test_no_synchro_process(int argc, char **argv) {
	char arg_buffer[32];
	DBG_ARGS(argc, argv);
	DBG_MSG("test_no_synchro_process start");
	
	printf("\n[test_no_synchro_process] Starting with argc=%d\n", argc);
	copy_arg_or_default(arg_buffer, sizeof(arg_buffer), argv, 0, "10");
	printf("[test_no_synchro_process] Using arg: %s\n", arg_buffer);
	free_spawn_args(argv, argc);

	char *args[3] = {arg_buffer, "0", NULL};
	printf("[test_no_synchro_process] Calling test_no_synchro...\n");
	test_no_synchro(2, args);
	printf("[test_no_synchro_process] Finished\n");
	sys_exit(0);
}

void test_synchro_process(int argc, char **argv) {
	char arg0_buffer[32];
	char arg1_buffer[32];
	DBG_ARGS(argc, argv);
	DBG_MSG("test_synchro_process start");
	
	printf("\n[test_synchro_process] Starting with argc=%d\n", argc);
	copy_arg_or_default(arg0_buffer, sizeof(arg0_buffer), argv, 0, "10");
	copy_arg_or_default(arg1_buffer, sizeof(arg1_buffer), argv, 1, "1");
	printf("[test_synchro_process] Using args: %s, %s\n", arg0_buffer, arg1_buffer);
	free_spawn_args(argv, argc);

	char *args[3] = {arg0_buffer, arg1_buffer, NULL};
	printf("[test_synchro_process] Calling test_synchro...\n");
	test_synchro(2, args);
	printf("[test_synchro_process] Finished\n");
	sys_exit(0);
}

// initialize all to 0
char line[MAX_BUFF + 1] = {0};
char parameter[MAX_BUFF + 1] = {0};
char command[MAX_BUFF + 1] = {0};
int terminate = 0;
int linePos = 0;
char lastc;
static char username[USERNAME_SIZE] = "user";
static char commandHistory[MAX_COMMAND][MAX_BUFF] = {0};
static int commandIterator = 0;
static int commandIdxMax = 0;
static int loop_counter = 0;

char usernameLength = 4;

// Declaraciones adelantadas
int isUpperArrow(char c);
int isDownArrow(char c);
static const char *state_to_string(int state);
static int next_token(const char *src, int *index, char *out, int max_len);
static int parse_int_token(const char *token, int *value);
static void loop_process(int argc, char **argv);

// Forward declarations for cmd functions
void cmd_undefined(void);
void cmd_help(void);
void cmd_time(void);
void cmd_clear(void);
void cmd_registersinfo(void);
void cmd_zeroDiv(void);
void cmd_invOpcode(void);
void cmd_exit(void);
void cmd_ascii(void);
void cmd_eliminator(void);
void cmd_test_mm(void);
void cmd_test_processes(void);
void cmd_test_sync(void);
void cmd_test_no_synchro(void);
void cmd_test_synchro(void);
void cmd_debug(void);
void cmd_ps(void);
void cmd_loop(void);
void cmd_nice(void);
void cmd_kill(void);
void cmd_yield(void);
void cmd_cat(void);
void cmd_wc(void);
void cmd_filter(void);
void cmd_echo(void);

void printHelp()
{
	printsColor("\n\n===== Listing a preview of available commands =====\n", MAX_BUFF, GREEN);
	printsColor("\n>'help' or 'ls'     - displays this shell information", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>time               - display current time", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>clear              - clear the display", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>(+)                - increase font size (scaled)", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>(-)                - decrease font size (scaled)", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>registersinfo      - print current register values", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>zerodiv            - testeo divide by zero exception", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>invopcode          - testeo invalid op code exception", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>ps                 - list all processes", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>loop [-p prio]     - prints short greeting and process PID", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>nice <pid> <prio>  - change a given's process priority", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>kill <pid>         - kill specified process", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>yield              - yield the CPU", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>echo <text>        - print text to stdout", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>cat                - read from stdin and write to stdout", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>wc                 - count lines from stdin", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>filter             - remove vowels from stdin", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>eliminator         - launch ELIMINATOR videogame", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>test_mm [size]     - test memory manager (default: 100000000)", MAX_BUFF, YELLOW);
	printsColor("\n>test_processes [n] - test process management (default: 10)", MAX_BUFF, YELLOW);
	printsColor("\n>test_no_synchro [n]- run race condition without semaphores", MAX_BUFF, YELLOW);
	printsColor("\n>test_synchro [n] [u] - run synchronized version using semaphores", MAX_BUFF, YELLOW);
	printsColor("\n>test_sync [n] [u]  - alias for test_synchro", MAX_BUFF, YELLOW);
	printsColor("\n>debug [on|off]     - toggle debug logging", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>exit               - exit KERNEL OS", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n\n", MAX_BUFF, WHITE);
	printsColor("Examples:\n", MAX_BUFF, GREEN);
	printsColor("  test_mm 50000000       - test memory manager with 50MB\n", MAX_BUFF, WHITE);
	printsColor("  test_processes 5       - test with 5 processes\n", MAX_BUFF, WHITE);
	printsColor("  test_synchro 10 1      - synchronized test with params\n", MAX_BUFF, WHITE);
	printsColor("  loop -p 2              - spawn loop process with priority 2\n", MAX_BUFF, WHITE);
	printsColor("  nice 3 1               - change process 3 priority to 1\n", MAX_BUFF, WHITE);
	printsColor("\n", MAX_BUFF, WHITE);
	printsColor("Pipe examples:\n", MAX_BUFF, GREEN);
	printsColor("  echo hola | wc         - count lines in 'hola'\n", MAX_BUFF, CYAN);
	printsColor("  echo \"hola mundo\" | wc  - count lines with spaces\n", MAX_BUFF, CYAN);
	printsColor("  echo abracadabra | filter - remove vowels\n", MAX_BUFF, CYAN);
	printsColor("  cat | filter           - read input and filter vowels\n\n", MAX_BUFF, CYAN);
}

const char *commands[] = {"undefined", "help", "ls", "time", "clear", "registersinfo", "zerodiv", "invopcode", "exit", "ascii", "eliminator", "test_mm", "test_processes", "test_sync", "test_no_synchro", "test_synchro", "debug", "ps", "loop", "nice", "kill", "yield", "cat", "wc", "filter", "echo"};
static void (*commands_ptr[MAX_ARGS])() = {
	cmd_undefined,
	cmd_help,
	cmd_help,
	cmd_time,
	cmd_clear,
	cmd_registersinfo,
	cmd_zeroDiv,
	cmd_invOpcode,
	cmd_exit,
	cmd_ascii,
	cmd_eliminator,
	cmd_test_mm,
	cmd_test_processes,
	cmd_test_sync,
	cmd_test_no_synchro,
	cmd_test_synchro,
	cmd_debug,
	cmd_ps,
	cmd_loop,
	cmd_nice,
	cmd_kill,
	cmd_yield,
	cmd_cat,
	cmd_wc,
	cmd_filter,
	cmd_echo
};

void kitty()
{
	char c;
	printPrompt();

	while (1 && !terminate)
	{
		drawCursor();
		c = getChar();
		printLine(c);
	}
}

void printLine(char c)
{
	if (linePos >= MAX_BUFF || c == lastc)
	{
		return;
	}
	if (isChar(c) || c == ' ' || isDigit(c))
	{
		handleSpecialCommands(c);
	}
	else if (c == BACKSPACE && linePos > 0)
	{
		printc(c);
		line[--linePos] = 0;
	}
	else if (c == NEW_LINE)
	{
		newLine();
	}
	lastc = c;
}

// Detecta si hay pipe '|' en la línea
static int has_pipe(char *str) {
	for (int i = 0; str[i] != '\0'; i++) {
		if (str[i] == '|') {
			return i;
		}
	}
	return -1;
}

// Ejecuta comando con pipes
static void execute_pipe(char *left_cmd, char *right_cmd) {
	// Crear pipe nombrado temporal
	const char *pipe_name = "tmp_pipe";
	
	// Abrir pipe para escritura (comando izquierdo)
	int write_fd = sys_pipe_open(pipe_name, 2);  // 2 = WRITE
	if (write_fd < 0) {
		printsColor("\nError: failed to create pipe\n", MAX_BUFF, RED);
		return;
	}
	
	// Abrir pipe para lectura (comando derecho)
	int read_fd = sys_pipe_open(pipe_name, 1);   // 1 = READ
	if (read_fd < 0) {
		sys_pipe_close(write_fd);
		printsColor("\nError: failed to open pipe for reading\n", MAX_BUFF, RED);
		return;
	}
	
	// Ejecutar comando izquierdo (escribe al pipe)
	// Verificar si empieza con "echo "
	int is_echo = 0;
	if (left_cmd[0] == 'e' && left_cmd[1] == 'c' && left_cmd[2] == 'h' && 
	    left_cmd[3] == 'o' && left_cmd[4] == ' ') {
		is_echo = 1;
	}
	
	if (is_echo) {
		// echo: escribe el texto al pipe
		char *text = left_cmd + 5;
		// Remover comillas si existen
		if (text[0] == '"') {
			text++;
			int len = strlen(text);
			if (len > 0 && text[len-1] == '"') {
				text[len-1] = '\0';
			}
		}
		sys_write_fd(write_fd, text, strlen(text));
		sys_write_fd(write_fd, "\n", 1);
	} else if (strcmp(left_cmd, "cat") == 0) {
		// cat: lee stdin y escribe al pipe
		char buf[256];
		int n;
		while ((n = sys_read_fd(0, buf, sizeof(buf))) > 0) {
			sys_write_fd(write_fd, buf, n);
		}
	}
	
	// Cerrar escritura para indicar EOF
	sys_pipe_close(write_fd);
	
	// Ejecutar comando derecho (lee del pipe)
	if (strcmp(right_cmd, "wc") == 0) {
		char buf[256];
		int n;
		int lines = 0;
		while ((n = sys_read_fd(read_fd, buf, sizeof(buf))) > 0) {
			for (int i = 0; i < n; i++) {
				if (buf[i] == '\n') {
					lines++;
				}
			}
		}
		printf("%d\n", lines);
	} else if (strcmp(right_cmd, "filter") == 0) {
		char buf[256];
		int n;
		while ((n = sys_read_fd(read_fd, buf, sizeof(buf))) > 0) {
			for (int i = 0; i < n; i++) {
				char c = buf[i];
				// Solo imprimir si NO es vocal
				if (!(c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
				      c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U')) {
					printc(c);
				}
			}
		}
	}
	
	// Cerrar lectura
	sys_close_fd(read_fd);
	
	// Limpiar pipe
	sys_pipe_unlink(pipe_name);
}

void newLine()
{
	// Detectar si hay pipe
	int pipe_pos = has_pipe(line);
	
	if (pipe_pos >= 0) {
		// Separar comandos
		char left_cmd[MAX_BUFF] = {0};
		char right_cmd[MAX_BUFF] = {0};
		
		int i;
		for (i = 0; i < pipe_pos && line[i] != '\0'; i++) {
			left_cmd[i] = line[i];
		}
		left_cmd[i] = '\0';
		
		// Saltar el pipe y espacios
		i = pipe_pos + 1;
		while (i < linePos && line[i] == ' ') i++;
		
		int j = 0;
		while (i < linePos && line[i] != '\0') {
			right_cmd[j++] = line[i++];
		}
		right_cmd[j] = '\0';
		
		// Trim espacios
		int len = strlen(left_cmd);
		while (len > 0 && left_cmd[len-1] == ' ') {
			left_cmd[--len] = '\0';
		}
		len = strlen(right_cmd);
		while (len > 0 && right_cmd[len-1] == ' ') {
			right_cmd[--len] = '\0';
		}
		
		// Ejecutar con pipe
		execute_pipe(left_cmd, right_cmd);
	} else {
		// Ejecución normal sin pipe
		int i = checkLine();
		(*commands_ptr[i])();
	}

	for (int i = 0; line[i] != '\0'; i++)
	{
		line[i] = 0;
		command[i] = 0;
		parameter[i] = 0;
	}
	linePos = 0;

	prints("\n", MAX_BUFF);
	printPrompt();
}

void printPrompt()
{
	prints(username, usernameLength);
	prints(" $", MAX_BUFF);
	printcColor('>', PINK);
}

// separa comando de parametro
int checkLine()
{
	int i = 0;
	int j = 0;
	int k = 0;
	for (j = 0; j < linePos && line[j] != ' '; j++)
	{
		command[j] = line[j];
	}
	if (j < linePos)
	{
		j++;
		while (j < linePos)
		{
			parameter[k++] = line[j++];
		}
	}

	strcpyForParam(commandHistory[commandIdxMax++], command, parameter);
	commandIterator = commandIdxMax;

	int command_count = sizeof(commands) / sizeof(commands[0]);
	for (i = 1; i < command_count; i++)
	{
		if (strcmp(command, commands[i]) == 0)
		{
			return i;
		}
	}

	return 0;
}

void cmd_help()
{
	printsColor("\n\n===== Listing a preview of available commands =====\n", MAX_BUFF, GREEN);
	printHelp();
}

void cmd_undefined()
{
	prints("\n\nbash: command not found: \"", MAX_BUFF);
	prints(command, MAX_BUFF);
	prints("\" Use 'help' or 'ls' to display available commands", MAX_BUFF);
}

void cmd_time()
{
	getTime();
}

void cmd_exit()
{
	prints("\n\nExiting OS\n", MAX_BUFF);
	terminate = 1;
}

void cmd_clear()
{
	clear_scr();
}

void cmd_registersinfo()
{
	registerInfo();
}

void cmd_invOpcode()
{
	test_invopcode();
}

void cmd_zeroDiv()
{
	test_zerodiv();
}

void cmd_charsizeplus()
{
	cmd_clear();
	increaseScale();
	printPrompt();
}

void cmd_charsizeminus()
{
	cmd_clear();
	decreaseScale();
	printPrompt();
}

void handleSpecialCommands(char c)
{
	if (c == PLUS)
	{
		cmd_charsizeplus();
	}
	else if (c == MINUS)
	{
		cmd_charsizeminus();
	}
	else if (isUpperArrow(c))
	{
		historyCaller(-1);
	}
	else if (isDownArrow(c))
	{
		historyCaller(1);
	}
	else
	{
		line[linePos++] = c;
		printc(c);
	}
}

void cmd_eliminator()
{
	int numPlayers;
	if (parameter[0] == '\0')
	{
		numPlayers = 1;
	}
	else
	{
		numPlayers = atoi(parameter);
	}

	if (numPlayers == 1 || numPlayers == 2 || parameter[0] == '\0')
	{
		int playAgain = 1;
		while (playAgain)
		{
			// playAgain because we need to know if the game should be restarted
			playAgain = eliminator(numPlayers);
		}
	}
	else
	{
		prints("\nERROR: Invalid number of players. Only 1 or 2 players allowed.", MAX_BUFF);
	}
}

void cmd_test_mm()
{
	printsColor("\n========================================", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    MEMORY MANAGER TEST", MAX_BUFF, YELLOW);
	printsColor("\n========================================\n", MAX_BUFF, LIGHT_BLUE);
	
	char token[32];
	int idx = 0;
	int argc_spawn = 0;
	char **argv_spawn = NULL;

	if (next_token(parameter, &idx, token, sizeof(token))) {
		size_t len = strlen(token) + 1;
		char *arg0 = (char*)sys_malloc(len);
		if (arg0 == NULL) {
			printsColor("\nFailed to allocate args", MAX_BUFF, RED);
			return;
		}
		strcpy(arg0, token);

		argv_spawn = (char**)sys_malloc(sizeof(char*) * 2);
		if (argv_spawn == NULL) {
			sys_free(arg0);
			printsColor("\nFailed to allocate argv", MAX_BUFF, RED);
			return;
		}
		argv_spawn[0] = arg0;
		argv_spawn[1] = NULL;
		argc_spawn = 1;
		printsColor("Testing with size: ", MAX_BUFF, WHITE);
		printsColor(token, MAX_BUFF, GREEN);
		printsColor("\n", MAX_BUFF, WHITE);
	} else {
		printsColor("Testing with default size: 100000000\n", MAX_BUFF, WHITE);
	}

	printsColor("Spawning test_mm process...\n", MAX_BUFF, LIGHT_BLUE);
	int64_t pid = spawn_test_process("test_mm", test_mm_process, argc_spawn, argv_spawn);
	if (pid < 0) {
		printsColor("\n[ERROR] Failed to launch test_mm\n", MAX_BUFF, RED);
	} else {
		printsColor("CREATED 'test_mm' PROCESS (PID: ", MAX_BUFF, GREEN);
		printf("%d)\n", (int)pid);
		printsColor("Press Ctrl+C to stop if needed\n", MAX_BUFF, LIGHT_BLUE);
	}
}

void cmd_test_processes()
{
	printsColor("\n========================================", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    PROCESS MANAGEMENT TEST", MAX_BUFF, YELLOW);
	printsColor("\n========================================\n", MAX_BUFF, LIGHT_BLUE);
	printsColor("Running test_processes\n", MAX_BUFF, WHITE);
	printsColor("Press 1 or 2 to change process priorities or C to stop.\n", MAX_BUFF, ORANGE);
	
	char token[32];
	int idx = 0;
	int argc_spawn = 0;
	char **argv_spawn = NULL;

	if (next_token(parameter, &idx, token, sizeof(token))) {
		size_t len = strlen(token) + 1;
		char *arg0 = (char*)sys_malloc(len);
		if (arg0 == NULL) {
			printsColor("\nFailed to allocate args", MAX_BUFF, RED);
			return;
		}
		strcpy(arg0, token);

		argv_spawn = (char**)sys_malloc(sizeof(char*) * 2);
		if (argv_spawn == NULL) {
			sys_free(arg0);
			printsColor("\nFailed to allocate argv", MAX_BUFF, RED);
			return;
		}
		argv_spawn[0] = arg0;
		argv_spawn[1] = NULL;
		argc_spawn = 1;
		printsColor("Testing with ", MAX_BUFF, WHITE);
		printsColor(token, MAX_BUFF, GREEN);
		printsColor(" processes\n", MAX_BUFF, WHITE);
	} else {
		printsColor("Testing with 10 processes (default)\n", MAX_BUFF, WHITE);
	}

	printsColor("Spawning test_processes...\n", MAX_BUFF, LIGHT_BLUE);
	int64_t pid = spawn_test_process("test_processes", test_processes_process, argc_spawn, argv_spawn);
	if (pid < 0) {
		printsColor("\n[ERROR] Failed to launch test_processes\n", MAX_BUFF, RED);
	} else {
		printsColor("CREATED 'test_processes' PROCESS (PID: ", MAX_BUFF, GREEN);
		printf("%d)\n", (int)pid);
	}
}

void cmd_test_sync()
{
	cmd_test_synchro();
}

void cmd_test_no_synchro()
{
	printsColor("\n========================================", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n  RACE CONDITION TEST (NO SEMAPHORES)", MAX_BUFF, RED);
	printsColor("\n========================================\n", MAX_BUFF, LIGHT_BLUE);
	printsColor("Running test WITHOUT synchronization\n", MAX_BUFF, ORANGE);
	printsColor("Expect to see RACE CONDITIONS!\n", MAX_BUFF, LIGHT_RED);
	
	char token[32];
	int idx = 0;
	int argc_spawn = 0;
	char **argv_spawn = NULL;

	if (next_token(parameter, &idx, token, sizeof(token))) {
		size_t len = strlen(token) + 1;
		char *arg0 = (char*)sys_malloc(len);
		if (arg0 == NULL) {
			printsColor("\nFailed to allocate args", MAX_BUFF, RED);
			return;
		}
		strcpy(arg0, token);

		argv_spawn = (char**)sys_malloc(sizeof(char*) * 2);
		if (argv_spawn == NULL) {
			sys_free(arg0);
			printsColor("\nFailed to allocate argv", MAX_BUFF, RED);
			return;
		}
		argv_spawn[0] = arg0;
		argv_spawn[1] = NULL;
		argc_spawn = 1;
		printsColor("Testing with ", MAX_BUFF, WHITE);
		printsColor(token, MAX_BUFF, GREEN);
		printsColor(" iterations\n", MAX_BUFF, WHITE);
	} else {
		printsColor("Testing with 10 iterations (default)\n", MAX_BUFF, WHITE);
	}

	printsColor("Spawning test_no_synchro...\n", MAX_BUFF, LIGHT_BLUE);
	int64_t pid = spawn_test_process("test_no_synchro", test_no_synchro_process, argc_spawn, argv_spawn);
	if (pid < 0) {
		printsColor("\n[ERROR] Failed to launch test_no_synchro\n", MAX_BUFF, RED);
	} else {
		printsColor("CREATED 'test_no_synchro' PROCESS (PID: ", MAX_BUFF, GREEN);
		printf("%d)\n", (int)pid);
	}
}

void cmd_test_synchro()
{
	printsColor("\n========================================", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n SYNCHRONIZED TEST (WITH SEMAPHORES)", MAX_BUFF, GREEN);
	printsColor("\n========================================\n", MAX_BUFF, LIGHT_BLUE);
	printsColor("Running test WITH synchronization\n", MAX_BUFF, ORANGE);
	printsColor("Using semaphores to prevent race conditions\n", MAX_BUFF, LIGHT_GREEN);
	
	char token0[32];
	char token1[32];
	int idx = 0;
	int argc_spawn = 0;
	char **argv_spawn = NULL;

	int first = next_token(parameter, &idx, token0, sizeof(token0)) ? 1 : 0;
	int second = next_token(parameter, &idx, token1, sizeof(token1)) ? 1 : 0;

	if (first || second) {
		size_t count = 0;
		if (first) count++;
		if (second) count++;

		argv_spawn = (char**)sys_malloc(sizeof(char*) * (count + 1));
		if (argv_spawn == NULL) {
			printsColor("\nFailed to allocate argv", MAX_BUFF, RED);
			return;
		}

		size_t index = 0;
		if (first) {
			size_t len = strlen(token0) + 1;
			char *arg0 = (char*)sys_malloc(len);
			if (arg0 == NULL) {
				sys_free(argv_spawn);
				printsColor("\nFailed to allocate args", MAX_BUFF, RED);
				return;
			}
			strcpy(arg0, token0);
			argv_spawn[index++] = arg0;
		}
		if (second) {
			size_t len = strlen(token1) + 1;
			char *arg1 = (char*)sys_malloc(len);
			if (arg1 == NULL) {
				if (first) {
					sys_free(argv_spawn[0]);
				}
				sys_free(argv_spawn);
				printsColor("\nFailed to allocate args", MAX_BUFF, RED);
				return;
			}
			strcpy(arg1, token1);
			argv_spawn[index++] = arg1;
		}
		argv_spawn[index] = NULL;
		argc_spawn = (int)count;
		
		printsColor("Testing with params: ", MAX_BUFF, WHITE);
		if (first) {
			printsColor(token0, MAX_BUFF, GREEN);
		}
		if (second) {
			printsColor(" ", MAX_BUFF, WHITE);
			printsColor(token1, MAX_BUFF, GREEN);
		}
		printsColor("\n", MAX_BUFF, WHITE);
	} else {
		printsColor("Testing with default params: 10 1\n", MAX_BUFF, WHITE);
	}

	printsColor("Spawning test_synchro...\n", MAX_BUFF, LIGHT_BLUE);
	int64_t pid = spawn_test_process("test_synchro", test_synchro_process, argc_spawn, argv_spawn);
	if (pid < 0) {
		printsColor("\n[ERROR] Failed to launch test_synchro\n", MAX_BUFF, RED);
	} else {
		printsColor("CREATED 'test_synchro' PROCESS (PID: ", MAX_BUFF, GREEN);
		printf("%d)\n", (int)pid);
	}
}

void cmd_debug()
{
	if (parameter[0] == '\0') {
		printf("\nDebug logging is %s\n", debug_enabled ? "ON" : "OFF");
		return;
	}

	if (strcmp(parameter, "on") == 0) {
		debug_enabled = 1;
		printsColor("\nDebug logging enabled\n", MAX_BUFF, LIGHT_GREEN);
		DBG_MSG("debug mode enabled");
	} else if (strcmp(parameter, "off") == 0) {
		debug_enabled = 0;
		printsColor("\nDebug logging disabled\n", MAX_BUFF, LIGHT_RED);
	} else {
		printsColor("\nUsage: debug [on|off]\n", MAX_BUFF, ORANGE);
	}
}

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

static int next_token(const char *src, int *index, char *out, int max_len) {
	int i = *index;
	while (src[i] == ' ') {
		i++;
	}
	if (src[i] == '\0') {
		return 0;
	}
	int j = 0;
	while (src[i] != '\0' && src[i] != ' ' && j < max_len - 1) {
		out[j++] = src[i++];
	}
	out[j] = '\0';
	while (src[i] == ' ') {
		i++;
	}
	*index = i;
	return 1;
}

static int parse_int_token(const char *token, int *value) {
	if (token == NULL || token[0] == '\0') {
		return 0;
	}
	uint64_t parsed = charToInt((char *)token);
	if (parsed == (uint64_t)-1) {
		return 0;
	}
	*value = (int)parsed;
	return 1;
}

static void loop_process(int argc, char **argv) {
	(void)argc;
	(void)argv;
	int pid = (int)sys_getpid();
	while (1) {
		printf("[loop %d] .\n", pid);
		sys_wait(200);
	}
}

void cmd_ps()
{
	proc_info_t info[MAX_PROCS];
	int count = sys_proc_snapshot(info, MAX_PROCS);
	if (count <= 0)
	{
		prints("\nNo processes to show", MAX_BUFF);
		return;
	}

	printsColor("\nPID   PRIO STATE  TICKS FG NAME", MAX_BUFF, GREEN);
	prints("\n", MAX_BUFF);
	for (int i = 0; i < count; i++)
	{
		const char *state = state_to_string(info[i].state);
		const char *fg = info[i].fg ? "FG" : "BG";
		printf("PID %d PRIO %d STATE %s TICKS %d %s %s\n",
		       info[i].pid,
		       info[i].priority,
		       state,
		       info[i].ticks_left,
		       fg,
		       info[i].name);
	}
}

void cmd_loop()
{
	int prio = DEFAULT_PRIORITY;
	int idx = 0;
	char token[MAX_BUFF];

	if (parameter[0] != '\0')
	{
		if (!next_token(parameter, &idx, token, sizeof(token)))
		{
			printsColor("\nInvalid priority", MAX_BUFF, RED);
			return;
		}

		if (strcmp(token, "-p") == 0)
		{
			if (!next_token(parameter, &idx, token, sizeof(token)))
			{
				printsColor("\nUsage: loop [-p <prio>]", MAX_BUFF, RED);
				return;
			}
		}

		if (!parse_int_token(token, &prio))
		{
			printsColor("\nPriority must be a number", MAX_BUFF, RED);
			return;
		}
	}

	if (prio < MIN_PRIORITY || prio > MAX_PRIORITY)
	{
		printsColor("\nPriority out of range (0-3)", MAX_BUFF, RED);
		return;
	}

	char name[32];
	sprintf(name, "loop-%d", loop_counter++);
	int pid = sys_create_process(loop_process, 0, NULL, name, (uint8_t)prio);
	if (pid < 0)
	{
		printsColor("\nFailed to spawn loop", MAX_BUFF, RED);
		return;
	}
	printf("\nSpawned %s (pid %d, prio %d)\n", name, pid, prio);
}

void cmd_nice()
{
	int idx = 0;
	char token[MAX_BUFF];
	int pid;
	int prio;

	if (!next_token(parameter, &idx, token, sizeof(token)))
	{
		printsColor("\nUsage: nice <pid> <prio>", MAX_BUFF, RED);
		return;
	}

	if (!parse_int_token(token, &pid))
	{
		printsColor("\nInvalid pid", MAX_BUFF, RED);
		return;
	}

	if (!next_token(parameter, &idx, token, sizeof(token)))
	{
		printsColor("\nUsage: nice <pid> <prio>", MAX_BUFF, RED);
		return;
	}

	if (!parse_int_token(token, &prio))
	{
		printsColor("\nInvalid priority", MAX_BUFF, RED);
		return;
	}

	if (prio < MIN_PRIORITY || prio > MAX_PRIORITY)
	{
		printsColor("\nPriority out of range (0-3)", MAX_BUFF, RED);
		return;
	}

	sys_nice(pid, (uint8_t)prio);
	printf("\nSet pid %d priority to %d\n", pid, prio);
}

void cmd_kill()
{
	int idx = 0;
	char token[MAX_BUFF];
	int pid;

	if (!next_token(parameter, &idx, token, sizeof(token)))
	{
		printsColor("\nUsage: kill <pid>", MAX_BUFF, RED);
		return;
	}

	if (!parse_int_token(token, &pid))
	{
		printsColor("\nInvalid pid", MAX_BUFF, RED);
		return;
	}

	if (sys_kill(pid) < 0)
	{
		printsColor("\nKill failed", MAX_BUFF, RED);
	}
	else
	{
		printf("\nKilled pid %d\n", pid);
	}
}

void cmd_yield()
{
	sys_yield();
	prints("\nYield requested", MAX_BUFF);
}

void cmd_cat()
{
	prints("\n", MAX_BUFF);
	char *argv[1] = {"cat"};
	cat_main(1, argv);
}

void cmd_wc()
{
	prints("\n", MAX_BUFF);
	char *argv[1] = {"wc"};
	wc_main(1, argv);
}

void cmd_filter()
{
	prints("\n", MAX_BUFF);
	char *argv[1] = {"filter"};
	filter_main(1, argv);
}

void cmd_echo()
{
	prints("\n", MAX_BUFF);
	prints(parameter, MAX_BUFF);
	prints("\n", MAX_BUFF);
}

void historyCaller(int direction)
{
	cmd_clear();
	printPrompt();
	commandIterator += direction;
	prints(commandHistory[commandIterator], MAX_BUFF);
	strcpy(line, commandHistory[commandIterator]);
	linePos = strlen(commandHistory[commandIterator]);
}

void cmd_ascii()
{

	int asciiIdx = random();
	size_t splash_length = 0;
	while (ascii[asciiIdx][splash_length] != NULL)
	{
		splash_length++;
	}

    for (size_t i = 0; i < splash_length; i++)
	{
		printsColor(ascii[asciiIdx][i], MAX_BUFF, WHITE);
		printc('\n');
	}
}

void welcome()
{
	printsColor("\n    Welcome this efficient and simple operating system\n", MAX_BUFF, GREEN);
	
#ifdef USE_BUDDY_SYSTEM
	printsColor("    Memory Manager: BUDDY SYSTEM\n", MAX_BUFF, YELLOW);
#else
	printsColor("    Memory Manager: FIRST FIT\n", MAX_BUFF, YELLOW);
#endif

	printsColor("    Here's a list of available commands\n", MAX_BUFF, GREEN);
	printHelp();
}
