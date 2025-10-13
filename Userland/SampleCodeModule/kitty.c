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
void test_mm_wrapper(int argc, char **argv) {
    // Si no se pasa argumento, usar valor por defecto grande
    char *default_args[] = {"100000000"};
    if (argc > 0 && argv != NULL) {
        test_mm(argc, argv);
    } else {
        test_mm(1, default_args);
    }
}

void test_processes_wrapper(int argc, char **argv) {
    char *default_args[] = {"10"};
    if (argc > 0 && argv != NULL) {
        test_processes(argc, argv);
    } else {
        test_processes(1, default_args);
    }
}

void test_sync_wrapper(int argc, char **argv) {
    char *default_args[] = {"10", "1"};
    if (argc > 0 && argv != NULL) {
        test_sync(argc, argv);
    } else {
        test_sync(2, default_args);
    }
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

char usernameLength = 4;

// Declaraciones adelantadas
int isUpperArrow(char c);
int isDownArrow(char c);

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
	printsColor("\n    >eliminator         - launch ELIMINATOR videogame", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >test_mm            - test memory manager", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >test_processes     - test process management", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >test_sync          - test synchronization", MAX_BUFF, LIGHT_BLUE);
	printsColor("\n    >exit               - exit OS\n", MAX_BUFF, LIGHT_BLUE);

	printc('\n');
}

const char *commands[] = {"undefined", "help", "ls", "time", "clear", "registersinfo", "zerodiv", "invopcode", "exit", "ascii", "eliminator", "test_mm", "test_processes", "test_sync"};
static void (*commands_ptr[MAX_ARGS])() = {cmd_undefined, cmd_help, cmd_help, cmd_time, cmd_clear, cmd_registersinfo, cmd_zeroDiv, cmd_invOpcode, cmd_exit, cmd_ascii, cmd_eliminator, cmd_test_mm, cmd_test_processes, cmd_test_sync};

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

	for (i = 1; i < MAX_ARGS; i++)
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
	char *args[] = {NULL};
	int pid = sys_create_process(test_mm_wrapper, 0, args, "test_mm", 1);
	
	if (pid <= 0) {
		prints("ERROR: No se pudo crear el proceso del test\n", MAX_BUFF);
	}
}

void cmd_test_processes()
{
	char *args[] = {NULL};
	int pid = sys_create_process(test_processes_wrapper, 0, args, "test_processes", 1);
	
	if (pid <= 0) {
		prints("ERROR: No se pudo crear el proceso del test\n", MAX_BUFF);
	}
}

void cmd_test_sync()
{
	char *args[] = {NULL};
	int pid = sys_create_process(test_sync_wrapper, 0, args, "test_sync", 1);
	
	if (pid <= 0) {
		prints("ERROR: No se pudo crear el proceso del test\n", MAX_BUFF);
	}
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

	for (int i = 0; i < splash_length; i++)
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