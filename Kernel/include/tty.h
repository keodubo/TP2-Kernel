#pragma once

#include <stdbool.h>
#include <stdint.h>

struct tty;
typedef struct tty tty_t;

tty_t *tty_default(void);
int tty_read(tty_t *t, void *buf, int n);
int tty_write(tty_t *t, const void *buf, int n);
int tty_close(tty_t *t);
void tty_push_char(tty_t *t, char c);
void tty_handle_input(uint8_t scancode, char ascii);
