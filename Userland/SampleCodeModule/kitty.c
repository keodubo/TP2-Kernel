#include <userlib.h>
#include <stdio.h>
#include <time.h>
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

// Wrappers para ejecutar tests como procesos
void test_mm_process(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char *default_args[] = {"100000000"};
    test_mm(1, default_args);
    // Cuando termina, el proceso debe terminar
    while(1);
}

void test_processes_process(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char *default_args[] = {"10"};
    test_processes(1, default_args);
    while(1);
}

void test_sync_process(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char *default_args[] = {"10", "1"};
    test_sync(2, default_args);
    while(1);
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

void printHelp()
{
	printsColor("\n\n    >'help' or 'ls'     - displays this shell information", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >time               - display current time", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >clear              - clear the display", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >(+)                - increase font size (scaled)", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >(-)                - decrease font size (scaled)", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >registersinfo      - print current register values", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >zerodiv            - testeo divide by zero exception", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >invopcode          - testeo invalid op code exception", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >ps                - list active processes", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >loop [-p prio]    - spawn a CPU-bound process", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >nice <pid> <prio>  - change priority of a process", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >kill <pid>        - terminate a process", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >yield             - yield the CPU", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >eliminator         - launch ELIMINATOR videogame", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >test_mm            - test memory manager", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >test_processes     - test process management", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >test_sync          - test synchronization", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >exit               - exit OS\n", MAX_BUFF, LIGHT_BLUE);

	printc('\n');
}

const char *commands[] = {"undefined", "help", "ls", "time", "clear", "registersinfo", "zerodiv", "invopcode", "exit", "ascii", "eliminator", "test_mm", "test_processes", "test_sync", "ps", "loop", "nice", "kill", "yield"};
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
	cmd_ps,
	cmd_loop,
	cmd_nice,
	cmd_kill,
	cmd_yield
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

void newLine()
{
	int i = checkLine();

	(*commands_ptr[i])();

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
	char *args[] = {"100000000"};
	test_mm(1, args);
}

void cmd_test_processes()
{
	char *args[] = {"10"};
	test_processes(1, args);
}

void cmd_test_sync()
{
	char *args[] = {"10", "1"};
	test_sync(2, args);
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
