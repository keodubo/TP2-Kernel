#ifndef _USERLIB_H_
#define _USERLIB_H_

#include <stdint.h>
#include <colors.h>

// Librería de utilidades de userland: IO, cadenas y helpers gráficos

extern const Color BLACK;
extern const Color WHITE;
extern const Color RED;
extern const Color LIGHT_BLUE;
extern const Color BLUE;
extern const Color ORANGE;
extern const Color YELLOW;
extern const Color PURPLE;
extern const Color PINK;
extern const Color GREEN;
extern const Color LIGHT_RED;
extern const Color LIGHT_PURPLE;
extern const Color LIGHT_ORANGE;
extern const Color LIGHT_YELLOW;
extern const Color LIGHT_PINK;
extern const Color LIGHT_GREEN;
extern const Color CYAN;

typedef struct Note
{
    int tone;
    int duration;
} NoteType;

/* Prints a char in screen */
void printc(char c);

/* Prints a string in screen */
void prints(const char *str, int lenght);

/* Gets a single char from keyboard */
char getChar();

/*is c a char*/
int isChar(char c);

/*is digit??*/
int isDigit(char c);

int random();

/* returns lenght of a string*/
int strlen(const char *str);

/* sets memory to a value*/
void *memset(void *dest, int32_t c, uint64_t length);

/* compares str1 with str2*/
int strcmp(const char *str1, const char *str2);

/* copies src into dest*/
char *strcpy(char *dest, const char *src);

/* copies src's into dest*/
char *strcpyForParam(char *dest, const char *src, const char *src2);

/*parses a num string into uint64_t*/
uint64_t charToInt(char *str);

/* prints in different bases */
void printDec(uint64_t value);
void printHex(uint64_t value);
void printBin(uint64_t value);
void printBase(uint64_t value, uint32_t base);
void printsColor(const char *str, int lenght, Color color);

int atoi(const char *str);

void triggerSpeaker(uint32_t frequence, uint64_t duration);
void playMelody(NoteType *melody, int length);

void clear_scr();
int get_scrWidth();
int get_scrHeight();
void drawRectangle(int x, int y, int x2, int y2, Color color);
void wait(uint64_t ms);
void inforeg();
void test_invopcode();
void test_zerodiv();
// void sizePlus();
// void sizeMinus();
int print_mem(uint64_t mem);
void printcColor(char c, Color color);

void increaseScale();

void decreaseScale();

void drawCursor();

void welcome();

void registerInfo();

// Memory management functions
void* malloc(uint64_t size);
void free(void* ptr);

#endif /* _USERLIB_H_ */
