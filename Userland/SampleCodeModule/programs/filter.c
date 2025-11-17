// filter.c - Filtra vocales (a,e,i,o,u,A,E,I,O,U) de stdin
// Útil para demostrar pipelines: echo hola | filter
#include <stdio.h>
#include <sys_calls.h>
#include <spawn_args.h>

// Verifica si un carácter es una vocal (mayúscula o minúscula)
// Retorna 1 si es vocal, 0 si no lo es
static int is_vowel(char c) {
    return (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
            c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U');
}

// Función principal: lee de stdin, elimina vocales y escribe a stdout
void filter_main(int argc, char **argv) {
    char buf[256];   // Buffer de entrada
    char out[256];   // Buffer de salida (sin vocales)
    int n;

    // Leer de stdin en bloques
    while ((n = sys_read_fd(0, buf, sizeof(buf))) > 0) {
        int j = 0;
        // Copiar solo caracteres que no son vocales
        for (int i = 0; i < n; i++) {
            if (!is_vowel(buf[i])) {
                out[j++] = buf[i];
            }
        }
        // Escribir el resultado filtrado a stdout
        if (j > 0) {
            sys_write_fd(1, out, j);
        }
    }
    free_spawn_args(argv, argc);
    sys_exit(0);
}
