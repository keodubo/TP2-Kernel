// filter.c - Filtra vocales (a,e,i,o,u,A,E,I,O,U)
#include "include/stdio.h"
#include "include/sys_calls.h"

static int is_vowel(char c) {
    return (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
            c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U');
}

int filter_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    char buf[256];
    char out[256];
    int n;
    
    while ((n = sys_read_fd(0, buf, sizeof(buf))) > 0) {
        int j = 0;
        for (int i = 0; i < n; i++) {
            if (!is_vowel(buf[i])) {
                out[j++] = buf[i];
            }
        }
        if (j > 0) {
            sys_write_fd(1, out, j);
        }
    }
    
    return 0;
}
