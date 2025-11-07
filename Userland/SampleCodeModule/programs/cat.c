// cat.c - Lee de stdin y escribe a stdout
#include <stdio.h>
#include <sys_calls.h>

// Lee en bloques y solo imprime cuando detecta fin de línea o EOF
int cat_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    char buf[256];
    int used = 0;
    int n;
    
    while ((n = sys_read_fd(0, buf + used, (int)(sizeof(buf) - used))) > 0) {
        used += n;

        int segment_start = 0;
        for (int i = 0; i < used; i++) {
            if (buf[i] == '\n') {
                int len = i - segment_start + 1;
                sys_write_fd(1, buf + segment_start, len);
                segment_start = i + 1;
            }
        }
        
        if (segment_start > 0) {
            int remaining = used - segment_start;
            if (remaining > 0) {
                for (int j = 0; j < remaining; j++) {
                    buf[j] = buf[segment_start + j];
                }
            }
            used = remaining;
        }
        
        if (used == (int)sizeof(buf)) {
            sys_write_fd(1, buf, used);
            used = 0;
        }
    }
    
    if (n == 0 && used > 0) {
        // EOF: volcar lo que quedó sin salto de línea
        sys_write_fd(1, buf, used);
    }
    
    return 0;
}
