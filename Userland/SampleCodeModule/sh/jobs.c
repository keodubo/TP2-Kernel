#include "jobs.h"

// Variable global para trackear el proceso en foreground
static int64_t fg_pid = -1;

void jobs_set_fg(int64_t pid) {
    fg_pid = pid;
}

int64_t jobs_get_fg(void) {
    return fg_pid;
}

void jobs_clear_fg(void) {
    fg_pid = -1;
}
