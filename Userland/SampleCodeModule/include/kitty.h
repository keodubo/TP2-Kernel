#ifndef _KITTY_H_
#define _KITTY_H_

#include <stdio.h>
#include "userlib.h"
#define MAX_BUFF 254
#define MAX_COMMAND 17
#define MAX_ARGS 17
#define USERNAME_SIZE 16
#define NEW_LINE '\n'
#define BACKSPACE '\b'
#define PLUS '+'
#define MINUS '-'

void kitty();

void printHelp();
void newLine();
void printLine(char c);
int checkLine();
void cmd_undefined();
void cmd_help();
void cmd_time();
void cmd_clear();
void cmd_registersinfo();
void cmd_zeroDiv();
void cmd_invOpcode();
void cmd_exit();
void cmd_charsizeplus();
void cmd_charsizeminus();
void cmd_ascii();
void printPrompt();
void cmd_eliminator();
void historyCaller(int direction);
void handleSpecialCommands(char c);


#endif