// cat.c - Lee de stdin y escribe a stdout
#include "include/stdio.h"
#include "include/sys_calls.h"

int cat_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    char buf[256];
    int n;
    
    while ((n = sys_read_fd(0, buf, sizeof(buf))) > 0) {
        sys_write_fd(1, buf, n);
    }
    
    return 0;
}
