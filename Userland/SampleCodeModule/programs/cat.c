// cat.c - Lee de stdin y escribe a stdout
// Útil en pipes para pasar datos entre comandos
#include <stdio.h>
#include <sys_calls.h>
#include <spawn_args.h>

// Función principal de cat: lee de stdin y escribe a stdout
// Lee en bloques y procesa línea por línea para mejor rendimiento
void cat_main(int argc, char **argv) {
    char buf[256];
    int used = 0;  // Cantidad de bytes en el buffer
    int n;

    // Leer de stdin hasta EOF
    while ((n = sys_read_fd(0, buf + used, (int)(sizeof(buf) - used))) > 0) {
        used += n;

        // Buscar saltos de línea y escribir líneas completas
        int segment_start = 0;
        for (int i = 0; i < used; i++) {
            if (buf[i] == '\n') {
                int len = i - segment_start + 1;
                sys_write_fd(1, buf + segment_start, len);
                segment_start = i + 1;
            }
        }

        // Mover datos no procesados al inicio del buffer
        if (segment_start > 0) {
            int remaining = used - segment_start;
            if (remaining > 0) {
                for (int j = 0; j < remaining; j++) {
                    buf[j] = buf[segment_start + j];
                }
            }
            used = remaining;
        }

        // Si el buffer está lleno, volcarlo aunque no haya salto de línea
        if (used == (int)sizeof(buf)) {
            sys_write_fd(1, buf, used);
            used = 0;
        }
    }

    // Al llegar a EOF, escribir cualquier dato restante
    if (n == 0 && used > 0) {
        sys_write_fd(1, buf, used);
    }
    free_spawn_args(argv, argc);
    sys_exit(0);
}
