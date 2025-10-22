#pragma once

#include <stdbool.h>
#include <stdint.h>

struct tty;
typedef struct tty tty_t;

/**
 * @brief Obtiene la instancia singleton de la TTY por defecto
 * @return Puntero a la estructura tty_t
 */
tty_t *tty_default(void);

/**
 * @brief Lee datos del buffer de entrada de la TTY (bloqueante)
 * @param t Puntero a la TTY
 * @param buf Buffer de destino
 * @param n Número de bytes a leer
 * @return Número de bytes leídos, o -1 en error
 */
int tty_read(tty_t *t, void *buf, int n);

/**
 * @brief Escribe datos a la salida de la TTY
 * @param t Puntero a la TTY
 * @param buf Buffer de origen
 * @param n Número de bytes a escribir
 * @return Número de bytes escritos, o -1 en error
 */
int tty_write(tty_t *t, const void *buf, int n);

/**
 * @brief Cierra la TTY (no-op para compatibilidad)
 * @param t Puntero a la TTY
 * @return 0 en éxito
 */
int tty_close(tty_t *t);

/**
 * @brief Encola un carácter en el buffer de entrada de la TTY
 * @param t Puntero a la TTY
 * @param c Carácter a encolar
 */
void tty_push_char(tty_t *t, char c);

/**
 * @brief Maneja la entrada de teclado (llamada desde el driver)
 * @param scancode Código de escaneo (no usado actualmente)
 * @param ascii Carácter ASCII correspondiente
 */
void tty_handle_input(uint8_t scancode, char ascii);

/**
 * @brief Establece el proceso que tiene el control de la TTY (foreground)
 * @param pid PID del proceso foreground (o -1 para ninguno)
 */
void tty_set_foreground(int pid);

/**
 * @brief Verifica si un proceso puede leer de la TTY
 * @param pid PID del proceso a verificar
 * @return true si el proceso está en foreground, false en caso contrario
 */
bool tty_can_read(int pid);

/**
 * @brief Obtiene el PID del proceso foreground actual
 * @return PID del foreground, o -1 si no hay ninguno
 */
int tty_get_foreground(void);
