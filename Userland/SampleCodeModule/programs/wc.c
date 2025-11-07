// wc.c - Contador de lineas (Word Count)
#include <stdio.h>
#include <sys_calls.h>

int wc_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    char buf[256];
    int n;
    int lines = 0;
    
    while ((n = sys_read_fd(0, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                lines++;
            }
        }
    }
    
    printf("%d\n", lines);
    
    return 0;
}
