#pragma once

#include <stdbool.h>

struct tty;
typedef struct tty tty_t;

tty_t *tty_default(void);
int tty_read(tty_t *t, void *buf, int n);
int tty_write(tty_t *t, const void *buf, int n);
int tty_close(tty_t *t);
void tty_push_char(tty_t *t, char c);
