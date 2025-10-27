#include <userlib.h>
#include <stdio.h>
#include <time.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys_calls.h>
#include <colors.h>
#include <kitty.h>
#include <ascii.h>
#include "../tests/test_util.h"
#include "../tests/syscall.h"
#include "sh/jobs.h"

// Shell interactiva: parsea comandos, gestiona pipes y utilidades de tests

// Declaraciones de las funciones de test
uint64_t test_mm(uint64_t argc, char *argv[]);
uint64_t test_processes(uint64_t argc, char *argv[]);
uint64_t test_prio(uint64_t argc, char *argv[]);
uint64_t test_sync(uint64_t argc, char *argv[]);
uint64_t test_no_synchro(uint64_t argc, char *argv[]);
uint64_t test_synchro(uint64_t argc, char *argv[]);

// Declaraciones de los comandos de pipes
int cat_main(int argc, char **argv);
int wc_main(int argc, char **argv);
int filter_main(int argc, char **argv);

#define SHELL_STDIN 0
#define SHELL_STDOUT 1
#define SHELL_STDERR 2
#define SHELL_BACKUP_STDIN 60
#define SHELL_BACKUP_STDOUT 61
#define PIPE_TMP_NAME_LEN 32

// Helpers para pipelines y parsing de comandos
static void sanitize_echo_text(const char *src, char *dst, int max_len);
static int parse_command_line(const char *input, char *out_cmd, char *out_param);
static void echo_output(const char *param, int interactive);
static int run_pipeline_command(const char *cmd, const char *param);
static int has_pipe(char *str);
static void execute_pipe(char *left_line, char *right_line);

static void format_hex64(uint64_t value, char out[17]);
static void print_hex64(uint64_t value);

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
    // Crear el proceso como foreground para que la shell pueda matarlo con Ctrl+C
    int64_t pid = sys_create_process_ex(entry, argc, argv, name, DEFAULT_PRIORITY, 1);
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

void test_priority_process(int argc, char **argv) {
	char arg_buffer[32];
	DBG_ARGS(argc, argv);
	DBG_MSG("test_priority_process start");

	printf("\n[test_priority_process] Starting with argc=%d\n", argc);
	copy_arg_or_default(arg_buffer, sizeof(arg_buffer), argv, 0, "1500");
	printf("[test_priority_process] Using window: %s ms\n", arg_buffer);
	free_spawn_args(argv, argc);

	char *args[2] = {arg_buffer, NULL};
	printf("[test_priority_process] Calling test_prio...\n");
	test_prio(1, args);
	printf("[test_priority_process] Finished\n");
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
static char username[USERNAME_SIZE] = "user";
static char commandHistory[MAX_COMMAND][MAX_BUFF] = {0};
static int commandIterator = 0;
static int commandIdxMax = 0;
static int loop_counter = 0;
static int is_background = 0; // Flag para indicar si el comando se ejecuta en background

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
void cmd_test_mm(void);
void cmd_test_processes(void);
void cmd_test_priority(void);
void cmd_test_sync(void);
void cmd_test_no_synchro(void);
void cmd_test_synchro(void);
void cmd_debug(void);
void cmd_ps(void);
void cmd_loop(void);
void cmd_nice(void);
void cmd_kill(void);
void cmd_block(void);
void cmd_yield(void);
void cmd_shell(void);
void cmd_waitpid(void);
void cmd_cat(void);
void cmd_wc(void);
void cmd_filter(void);
void cmd_mem(void);
void cmd_echo(void);
void cmd_jobs(void);
void printPrompt(void);

extern int mem_command(int argc, char **argv);

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
	printsColor("\n>jobs               - list background processes", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>sh                 - start new shell with FG/BG support (use '&' for background)", MAX_BUFF, LIGHT_GREEN);
	printsColor("\n>nice <pid> <prio>  - change a given's process priority", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>kill <pid>         - kill specified process", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>block <pid>        - toggle process between BLOCKED and READY", MAX_BUFF, LIGHT_BLUE);
    printsColor("\n>yield              - yield the CPU", MAX_BUFF, LIGHT_BLUE);
    printsColor("\n>waitpid <pid|-1>  - wait for a child to finish", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>echo <text>        - print text to stdout", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>mem [-v]           - show memory usage statistics", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>cat                - read from stdin and write to stdout", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>wc                 - count lines from stdin", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>filter             - remove vowels from stdin", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n>test_mm [size]     - test memory manager (default: 100000000)", MAX_BUFF, YELLOW);
    printsColor("\n>test_processes [n] - test process management (default: 10)", MAX_BUFF, YELLOW);
    printsColor("\n>test_priority [ms]- scheduling demo (default: 1500)", MAX_BUFF, YELLOW);
	printsColor("\n>test_no_synchro [n]- run race condition without semaphores", MAX_BUFF, YELLOW);
	printsColor("\n>test_synchro [n] [u] - run synchronized version using semaphores", MAX_BUFF, YELLOW);
	printsColor("\n>exit               - exit KERNEL OS", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n\n", MAX_BUFF, WHITE);
	printsColor("Background execution:\n", MAX_BUFF, GREEN);
	printsColor("  cmd &  - Execute command in background (no wait)\n", MAX_BUFF, CYAN);
	printsColor("Pipe examples:\n", MAX_BUFF, GREEN);
	printsColor("  echo hola | wc         - count lines in 'hola'\n", MAX_BUFF, CYAN);
	printsColor("  echo \"hola mundo\" | wc  - count lines with spaces\n", MAX_BUFF, CYAN);
	printsColor("  echo abracadabra | filter - remove vowels\n", MAX_BUFF, CYAN);
	printsColor("  cat | filter           - read input and filter vowels\n\n", MAX_BUFF, CYAN);
}

const char *commands[] = {"undefined", "help", "ls", "time", "clear", "registersinfo", "zerodiv", "invopcode", "exit", "ascii", "test_mm", "test_processes", "test_priority", "test_sync", "test_no_synchro", "test_synchro", "debug", "ps", "loop", "nice", "kill", "block", "yield", "waitpid", "mem", "cat", "wc", "filter", "echo", "jobs", "sh"};
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
	cmd_test_mm,
	cmd_test_processes,
	cmd_test_priority,
	cmd_test_sync,
	cmd_test_no_synchro,
	cmd_test_synchro,
	cmd_debug,
	cmd_ps,
	cmd_loop,
	cmd_nice,
	cmd_kill,
	cmd_block,
	cmd_yield,
	cmd_waitpid,
	cmd_mem,
	cmd_cat,
	cmd_wc,
	cmd_filter,
	cmd_echo,
	cmd_jobs,
	cmd_shell
};

// Bucle principal de la shell: lee caracteres y procesa líneas completas
void kitty()
{
	char c;
	jobs_init();
	printPrompt();

	while (1 && !terminate)
	{
		// Recolectar zombies de procesos background
		jobs_reap_background();
		
		drawCursor();
		c = getChar();
		printLine(c);
	}
}

// Actualiza el buffer de línea según la tecla recibida
void printLine(char c)
{
	if (c == 0)
	{
		return;
	}
	if (linePos >= MAX_BUFF)
	{
		return;
	}
	if (isChar(c) || c == ' ' || isDigit(c))
	{
		handleSpecialCommands(c);
	}
	else if (c == BACKSPACE && linePos > 0)
	{
		printc('\b');
		line[--linePos] = 0;
	}
	else if (c == NEW_LINE)
	{
		newLine();
	}
}

// Detecta si hay pipe '|' en la línea
// Recorta espacios y comillas de los textos de echo
static void sanitize_echo_text(const char *src, char *dst, int max_len) {
	if (dst == NULL || max_len <= 0) {
		return;
	}
	dst[0] = 0;
	if (src == NULL) {
		return;
	}

	int start = 0;
	while (src[start] == ' ') {
		start++;
	}

	int end = strlen(src);
	while (end > start && (src[end - 1] == ' ' || src[end - 1] == '\n' || src[end - 1] == '\r' || src[end - 1] == '\t')) {
		end--;
	}

	if (end - start >= 2 && src[start] == '"' && src[end - 1] == '"') {
		start++;
		end--;
	}

	int len = end - start;
	if (len <= 0) {
		dst[0] = 0;
		return;
	}
	if (len > max_len - 1) {
		len = max_len - 1;
	}

	for (int i = 0; i < len; i++) {
		dst[i] = src[start + i];
	}
	dst[len] = 0;
}

// Divide "cmd arg" en comando y parámetros sencillos
static int parse_command_line(const char *input, char *out_cmd, char *out_param) {
	if (input == NULL || out_cmd == NULL || out_param == NULL) {
		return -1;
	}

	int i = 0;
	while (input[i] == ' ') {
		i++;
	}

	int cmd_len = 0;
	while (input[i] != 0 && input[i] != ' ') {
		if (cmd_len < MAX_BUFF) {
			out_cmd[cmd_len++] = input[i];
		}
		i++;
	}
	out_cmd[cmd_len] = 0;

	while (input[i] == ' ') {
		i++;
	}

	int param_len = 0;
	while (input[i] != 0) {
		if (param_len < MAX_BUFF) {
			out_param[param_len++] = input[i];
		}
		i++;
	}
	while (param_len > 0 && out_param[param_len - 1] == ' ') {
		param_len--;
	}
	out_param[param_len] = 0;

	return (cmd_len > 0) ? 0 : -1;
}

// Imprime el texto de echo, preservando salto final cuando corresponde
static void echo_output(const char *param, int interactive) {
	char text[MAX_BUFF + 1];
	sanitize_echo_text(param, text, sizeof(text));

	const char newline = '\n';
	if (interactive) {
		sys_write_fd(SHELL_STDOUT, &newline, 1);
	}

	int len = strlen(text);
	if (len > 0) {
		sys_write_fd(SHELL_STDOUT, text, len);
	}
	sys_write_fd(SHELL_STDOUT, &newline, 1);
}

// Detecta si hay un operador '|' (pipe) en la linea
// Retorna la posicion del pipe o -1 si no hay
static int has_pipe(char *str) {
	for (int i = 0; str[i] != '\0'; i++) {
		if (str[i] == '|') {
			return i;
		}
	}
	return -1;
}

// Ejecuta la implementación real de cada comando soportado en pipelines
static int run_pipeline_command(const char *cmd, const char *param) {
	if (cmd == NULL || cmd[0] == 0) {
		return -1;
	}

	if (strcmp(cmd, "cat") == 0) {
		char *argv_cat[1] = {"cat"};
		return cat_main(1, argv_cat);
	}

	if (strcmp(cmd, "wc") == 0) {
		char *argv_wc[1] = {"wc"};
		return wc_main(1, argv_wc);
	}

	if (strcmp(cmd, "filter") == 0) {
		char *argv_filter[1] = {"filter"};
		return filter_main(1, argv_filter);
	}

	if (strcmp(cmd, "echo") == 0) {
		echo_output(param, 0);
		return 0;
	}

	printsColor("\n[pipe] Unsupported command in pipeline\n", MAX_BUFF, RED);
	return -1;
}

// Ejecuta dos comandos conectados por un pipe
// Crea un pipe temporal, redirige stdout del primer comando al pipe,
// redirige stdin del segundo comando del pipe, y ejecuta secuencialmente
static void execute_pipe(char *left_line, char *right_line) {
	char left_cmd[MAX_BUFF + 1] = {0};
	char left_param[MAX_BUFF + 1] = {0};
	char right_cmd[MAX_BUFF + 1] = {0};
	char right_param[MAX_BUFF + 1] = {0};

	if (parse_command_line(left_line, left_cmd, left_param) < 0 ||
	    parse_command_line(right_line, right_cmd, right_param) < 0) {
		printsColor("\n[pipe] Invalid command syntax\n", MAX_BUFF, RED);
		return;
	}

	static int pipe_seq = 0;
	char pipe_name[PIPE_TMP_NAME_LEN];
	sprintf(pipe_name, "tmp_pipe_%d", pipe_seq++);

	int backup_in = -1;
	int backup_out = -1;
	int write_fd = -1;
	int read_fd = -1;
	int status = 0;
	int pipe_created = 0;

	backup_in = sys_dup2(SHELL_STDIN, SHELL_BACKUP_STDIN);
	if (backup_in < 0) {
		printsColor("\n[pipe] Failed to backup stdin\n", MAX_BUFF, RED);
		status = -1;
		goto cleanup;
	}

	backup_out = sys_dup2(SHELL_STDOUT, SHELL_BACKUP_STDOUT);
	if (backup_out < 0) {
		printsColor("\n[pipe] Failed to backup stdout\n", MAX_BUFF, RED);
		status = -1;
		goto cleanup;
	}

	write_fd = sys_pipe_open(pipe_name, 2);
	if (write_fd < 0) {
		printsColor("\n[pipe] Failed to open pipe writer\n", MAX_BUFF, RED);
		status = -1;
		goto cleanup;
	}

	read_fd = sys_pipe_open(pipe_name, 1);
	if (read_fd < 0) {
		printsColor("\n[pipe] Failed to open pipe reader\n", MAX_BUFF, RED);
		status = -1;
		goto cleanup;
	}
	pipe_created = 1;

	if (sys_dup2(write_fd, SHELL_STDOUT) < 0) {
		printsColor("\n[pipe] Failed to redirect stdout\n", MAX_BUFF, RED);
		status = -1;
		goto cleanup;
	}
	sys_pipe_close(write_fd);
	write_fd = -1;

	if (run_pipeline_command(left_cmd, left_param) < 0) {
		status = -1;
	}

	if (backup_out >= 0) {
		sys_dup2(backup_out, SHELL_STDOUT);
		sys_close_fd(backup_out);
		backup_out = -1;
	}

	if (status == 0) {
		if (sys_dup2(read_fd, SHELL_STDIN) < 0) {
			printsColor("\n[pipe] Failed to redirect stdin\n", MAX_BUFF, RED);
			status = -1;
		} else {
			sys_pipe_close(read_fd);
			read_fd = -1;
			if (run_pipeline_command(right_cmd, right_param) < 0) {
				status = -1;
			}
		}
	}

cleanup:
	if (backup_in >= 0) {
		sys_dup2(backup_in, SHELL_STDIN);
		sys_close_fd(backup_in);
		backup_in = -1;
	}

	if (backup_out >= 0) {
		sys_dup2(backup_out, SHELL_STDOUT);
		sys_close_fd(backup_out);
		backup_out = -1;
	}

	if (read_fd >= 0) {
		sys_pipe_close(read_fd);
	}

	if (write_fd >= 0) {
		sys_pipe_close(write_fd);
	}

	if (pipe_created) {
		sys_pipe_unlink(pipe_name);
	}

	if (status < 0) {
		printsColor("\n[pipe] Execution failed\n", MAX_BUFF, RED);
	}
}

// Ejecuta la línea actual (con o sin pipeline) y reinicia el prompt
void newLine()
{
	// Detectar si hay '&' al final (background)
	is_background = 0;
	int effective_len = linePos;
	
	// Trim espacios al final
	while (effective_len > 0 && line[effective_len - 1] == ' ') {
		effective_len--;
	}
	
	// Verificar si termina en '&'
	if (effective_len > 0 && line[effective_len - 1] == '&') {
		is_background = 1;
		line[effective_len - 1] = '\0';  // Remover el '&'
		linePos = effective_len - 1;
		
		// Trim espacios después de remover '&'
		while (linePos > 0 && line[linePos - 1] == ' ') {
			line[linePos - 1] = '\0';
			linePos--;
		}
	}
	
	// Detectar si hay pipe
	int pipe_pos = has_pipe(line);
	if (pipe_pos >= 0) {
		char left_cmd[MAX_BUFF] = {0};
		char right_cmd[MAX_BUFF] = {0};

		int i;
		for (i = 0; i < pipe_pos && line[i] != '\0'; i++) {
			left_cmd[i] = line[i];
		}
		left_cmd[i] = '\0';

		i = pipe_pos + 1;
		while (i < linePos && line[i] == ' ') {
			i++;
		}

		int j = 0;
		while (i < linePos && line[i] != '\0') {
			right_cmd[j++] = line[i++];
		}
		right_cmd[j] = '\0';

		int len = strlen(left_cmd);
		while (len > 0 && left_cmd[len - 1] == ' ') {
			left_cmd[--len] = '\0';
		}
		len = strlen(right_cmd);
		while (len > 0 && right_cmd[len - 1] == ' ') {
			right_cmd[--len] = '\0';
		}

		if (left_cmd[0] == '\0' || right_cmd[0] == '\0') {
			printsColor("\n[pipe] Invalid command syntax\n", MAX_BUFF, RED);
		} else if (has_pipe(right_cmd) >= 0) {
			printsColor("\n[pipe] Only single '|' supported\n", MAX_BUFF, RED);
		} else {
			execute_pipe(left_cmd, right_cmd);
		}
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

	// Solo imprimir nueva línea y prompt si NO es background
	// (en background, el comando ya imprimió el mensaje [bg] y prompt)
	if (!is_background) {
		printc('\n');
		printPrompt();
	}
	// Resetear flag para siguiente comando
	is_background = 0;
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

// Inserta caracteres imprimibles o maneja atajos (historial, tamaño fuente)
void handleSpecialCommands(char c)
{
	if (c == PLUS && linePos == 0)
	{
		cmd_charsizeplus();
	}
	else if (c == MINUS && linePos == 0)
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
		
		// Esperar a que termine (automáticamente en foreground)
		int status = 0;
		sys_wait_pid(pid, &status);
		printsColor("[test_mm finished]\n", MAX_BUFF, LIGHT_BLUE);
		
		// Limpiar buffer de línea y mostrar prompt
		for (int i = 0; i < MAX_BUFF; i++) {
			line[i] = 0;
			command[i] = 0;
			parameter[i] = 0;
		}
		linePos = 0;
		printPrompt();
	}
}

void cmd_test_processes()
{
	printsColor("\n========================================", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    PROCESS MANAGEMENT TEST", MAX_BUFF, YELLOW);
	printsColor("\n========================================\n", MAX_BUFF, LIGHT_BLUE);
	printsColor("Running test_processes\n", MAX_BUFF, WHITE);
	printsColor("Press Ctrl+C to stop the test.\n", MAX_BUFF, ORANGE);
	
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
		
		// Esperar a que termine (automáticamente en foreground)
		int status = 0;
		sys_wait_pid(pid, &status);
		printsColor("[test_processes finished]\n", MAX_BUFF, LIGHT_BLUE);
		
		// Limpiar buffer de línea y mostrar prompt
		for (int i = 0; i < MAX_BUFF; i++) {
			line[i] = 0;
			command[i] = 0;
			parameter[i] = 0;
		}
		linePos = 0;
		printPrompt();
	}
}

void cmd_test_priority()
{
	printsColor("\n========================================", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    PRIORITY SCHEDULING TEST", MAX_BUFF, YELLOW);
	printsColor("\n========================================\n", MAX_BUFF, LIGHT_BLUE);
	printsColor("Running test_priority\n", MAX_BUFF, WHITE);
	printsColor("Default window: 1500 ms (override with argument).\n", MAX_BUFF, ORANGE);

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
		printsColor("Scheduling window: ", MAX_BUFF, WHITE);
		printsColor(token, MAX_BUFF, GREEN);
		printsColor(" ms\n", MAX_BUFF, WHITE);
	} else {
		printsColor("Scheduling window: 1500 ms (default)\n", MAX_BUFF, WHITE);
	}

	printsColor("Spawning test_priority...\n", MAX_BUFF, LIGHT_BLUE);
	int64_t pid = spawn_test_process("test_priority", test_priority_process, argc_spawn, argv_spawn);
	if (pid < 0) {
		printsColor("\n[ERROR] Failed to launch test_priority\n", MAX_BUFF, RED);
	} else {
		printsColor("CREATED 'test_priority' PROCESS (PID: ", MAX_BUFF, GREEN);
		printf("%d)\n", (int)pid);
		printsColor("Observe the counters to compare priorities.\n", MAX_BUFF, LIGHT_BLUE);
		
		// Esperar a que termine (automáticamente en foreground)
		int status = 0;
		sys_wait_pid(pid, &status);
		printsColor("[test_priority finished]\n", MAX_BUFF, LIGHT_BLUE);
		
		// Limpiar buffer de línea y mostrar prompt
		for (int i = 0; i < MAX_BUFF; i++) {
			line[i] = 0;
			command[i] = 0;
			parameter[i] = 0;
		}
		linePos = 0;
		printPrompt();
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
		
		// Esperar a que termine (automáticamente en foreground)
		int status = 0;
		sys_wait_pid(pid, &status);
		printsColor("[test_no_synchro finished]\n", MAX_BUFF, LIGHT_BLUE);
		
		// Limpiar buffer de línea y mostrar prompt
		for (int i = 0; i < MAX_BUFF; i++) {
			line[i] = 0;
			command[i] = 0;
			parameter[i] = 0;
		}
		linePos = 0;
		printPrompt();
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
		
		// Esperar a que termine (automáticamente en foreground)
		int status = 0;
		sys_wait_pid(pid, &status);
		printsColor("[test_synchro finished]\n", MAX_BUFF, LIGHT_BLUE);
		
		// Limpiar buffer de línea y mostrar prompt
		for (int i = 0; i < MAX_BUFF; i++) {
			line[i] = 0;
			command[i] = 0;
			parameter[i] = 0;
		}
		linePos = 0;
		printPrompt();
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
	int sign = 1;
	if (token[0] == '-') {
		sign = -1;
		token++;
		if (token[0] == '\0') {
			return 0;
		}
	}
	uint64_t parsed = charToInt((char *)token);
	if (parsed == (uint64_t)-1) {
		return 0;
	}
	if (sign < 0) {
		*value = -(int)parsed;
	} else {
		*value = (int)parsed;
	}
	return 1;
}

static void loop_process(int argc, char **argv) {
	int pid = (int)sys_getpid();
	int silent = 0;
	
	// Si se pasó argumento "silent", no imprimir
	if (argc > 0 && argv != NULL && argv[0] != NULL) {
		if (strcmp(argv[0], "silent") == 0) {
			silent = 1;
		}
	}
	
	while (1) {
		if (!silent) {
			printf("[loop %d] .\n", pid);
		}
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

	printsColor("\nPID   PRIO STATE  TICKS FG        SP                BP                NAME", MAX_BUFF, GREEN);
	prints("\n", MAX_BUFF);
	for (int i = 0; i < count; i++)
	{
		const char *state = state_to_string(info[i].state);
		const char *fg = info[i].fg ? "FG" : "BG";
		printf("PID %d PRIO %d STATE %s TICKS %d %s ", info[i].pid, info[i].priority, state, info[i].ticks_left, fg);
		print_hex64(info[i].sp);
		printc(' ');
		print_hex64(info[i].bp);
		printf(" %s\n", info[i].name);
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
	
	int pid;
	if (is_background)
	{
		// Background: crear con argumento "silent" para suprimir output
		char **args = (char**)sys_malloc(2 * sizeof(char*));
		if (args != NULL) {
			args[0] = (char*)sys_malloc(7);
			if (args[0] != NULL) {
				strcpy(args[0], "silent");
			}
			args[1] = NULL;
			pid = sys_create_process_ex(loop_process, 1, args, name, (uint8_t)prio, 0);
		} else {
			pid = sys_create_process_ex(loop_process, 0, NULL, name, (uint8_t)prio, 0);
		}
	}
	else
	{
		// Foreground: crear sin argumentos (output normal) y tomar la TTY
		pid = sys_create_process_ex(loop_process, 0, NULL, name, (uint8_t)prio, 1);
	}
	
	if (pid < 0)
	{
		printsColor("\nFailed to spawn loop", MAX_BUFF, RED);
		return;
	}
	
	if (is_background)
	{
		// Background: no wait, agregar a jobs
		jobs_add(pid, name);
		// Imprimir prompt inmediatamente para que el usuario pueda continuar
		printPrompt();
	}
	else
	{
		// Foreground: esperar a que termine
		printf("\nSpawned %s (pid %d, prio %d)\n", name, pid, prio);
		int status = 0;
		sys_wait_pid(pid, &status);
		printf("[loop %d terminated]\n", pid);
		
		// Limpiar buffer de línea y mostrar prompt
		for (int i = 0; i < MAX_BUFF; i++) {
			line[i] = 0;
			command[i] = 0;
			parameter[i] = 0;
		}
		linePos = 0;
		printPrompt();
	}
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

void cmd_block()
{
	int idx = 0;
	char token[MAX_BUFF];
	int pid;

	if (!next_token(parameter, &idx, token, sizeof(token)))
	{
		printsColor("\nUsage: block <pid>", MAX_BUFF, RED);
		return;
	}

	if (!parse_int_token(token, &pid) || pid <= 0)
	{
		printsColor("\nInvalid pid", MAX_BUFF, RED);
		return;
	}

	int self_pid = (int)sys_getpid();
	if (pid == self_pid)
	{
		printsColor("\nblock: refusing to block the current shell", MAX_BUFF, ORANGE);
		return;
	}

	proc_info_t procs[MAX_PROCS];
	int count = sys_proc_snapshot(procs, MAX_PROCS);
	if (count <= 0)
	{
		printsColor("\nblock: could not read process list", MAX_BUFF, RED);
		return;
	}

	proc_info_t *target = NULL;
	for (int i = 0; i < count; i++)
	{
		if (procs[i].pid == pid)
		{
			target = &procs[i];
			break;
		}
	}

	if (target == NULL)
	{
		printsColor("\nblock: pid not found", MAX_BUFF, RED);
		return;
	}

	if (target->state == 4)
	{
		printsColor("\nblock: process already exited", MAX_BUFF, RED);
		return;
	}

	if (target->state == 3)
	{
		if (sys_unblock(pid) < 0)
		{
			printsColor("\nblock: failed to unblock process", MAX_BUFF, RED);
			return;
		}
		printf("\nProcess %d moved to READY\n", pid);
		return;
	}

	if (target->state == 0)
	{
		printsColor("\nblock: process not started yet", MAX_BUFF, ORANGE);
		return;
	}

	if (sys_block(pid) < 0)
	{
		printsColor("\nblock: failed to block process", MAX_BUFF, RED);
		return;
	}

	printf("\nProcess %d moved to BLOCKED\n", pid);
}

void cmd_yield()
{
	sys_yield();
	prints("\nYield requested", MAX_BUFF);
}

void cmd_waitpid()
{
	int pid = -1;
	int idx = 0;
	char token[32];

	if (parameter[0] != '\0')
	{
		if (!next_token(parameter, &idx, token, sizeof(token)))
		{
			printsColor("\nUsage: waitpid <pid|-1>", MAX_BUFF, RED);
			return;
		}

		if (!parse_int_token(token, &pid))
		{
			printsColor("\nInvalid pid", MAX_BUFF, RED);
			return;
		}
	}

	int status = 0;
	int64_t waited = sys_wait_pid(pid, &status);
	if (waited < 0)
	{
		printsColor("\nwaitpid failed (no matching child)", MAX_BUFF, RED);
		return;
	}

	printf("\nwaitpid -> child %d exited with status %d\n", (int)waited, status);
}

void cmd_mem()
{
	char *argvv[3] = {"mem", NULL, NULL};
	char arg_buf[64];
	int idx = 0;
	int argc_local = 1;

	if (parameter[0] != '\0') {
		if (!next_token(parameter, &idx, arg_buf, sizeof(arg_buf))) {
			printsColor("\nmem: invalid argument\n", MAX_BUFF, RED);
			return;
		}
		argvv[argc_local++] = arg_buf;

		char extra[16];
		if (next_token(parameter, &idx, extra, sizeof(extra))) {
			printsColor("\nmem: too many arguments\n", MAX_BUFF, RED);
			return;
		}
	}

	int status = mem_command(argc_local, argvv);
	if (status != 0) {
		printf("\nmem exited with status %d\n", status);
	}
}

void cmd_cat()
{
	char *argv[1] = {"cat"};
	cat_main(1, argv);
}

void cmd_wc()
{
	char *argv[1] = {"wc"};
	wc_main(1, argv);
}

void cmd_filter()
{
	char *argv[1] = {"filter"};
	filter_main(1, argv);
}

void cmd_echo()
{
	echo_output(parameter, 1);
}

void cmd_jobs()
{
	jobs_list();
}

void cmd_shell()
{
	extern void shell_main(int argc, char **argv);
	
	printsColor("\nStarting new shell with FG/BG support...\n", MAX_BUFF, LIGHT_BLUE);
	printsColor("Type 'help' in the new shell for commands\n", MAX_BUFF, CYAN);
	printsColor("Use '&' at the end of commands to run in background\n\n", MAX_BUFF, CYAN);
	
	shell_main(0, NULL);
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
	prints("0x", MAX_BUFF);
	prints(buf, MAX_BUFF);
}
