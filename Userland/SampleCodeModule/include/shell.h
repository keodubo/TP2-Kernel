#ifndef SHELL_H
#define SHELL_H

/**
 * @file shell.h
 * @brief Interfaz de la shell con soporte para foreground/background
 */

/**
 * @brief Inicia la shell interactiva
 * @param argc Número de argumentos
 * @param argv Array de argumentos
 */
void shell_main(int argc, char **argv);

/**
 * @brief Parsea y ejecuta un comando
 * @param line Línea de comando a ejecutar
 * @return 0 en éxito, -1 en error
 */
int shell_execute(char *line);

#endif /* SHELL_H */
