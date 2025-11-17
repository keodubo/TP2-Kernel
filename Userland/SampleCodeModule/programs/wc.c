// wc.c - Contador de líneas (Word Count)
// Lee de stdin y cuenta la cantidad de saltos de línea
#include <stdio.h>
#include <sys_calls.h>
#include <spawn_args.h>

// Función principal de wc: cuenta líneas en stdin
// Útil en pipes: echo "texto\nmultilinea" | wc
void wc_main(int argc, char **argv) {
    char buf[256];
    int n;
    int lines = 0;

    // Leer de stdin hasta EOF
    while ((n = sys_read_fd(0, buf, sizeof(buf))) > 0) {
        // Contar cada '\n' encontrado
        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                lines++;
            }
        }
    }

    // Imprimir el total de líneas
    printf("%d\n", lines);

    free_spawn_args(argv, argc);
    sys_exit(0);
}
