#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stddef.h>

int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int puts(const char *str);
int putchar(int c);

#endif
