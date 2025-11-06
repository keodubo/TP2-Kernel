#ifndef MVAR_H
#define MVAR_H

#include <stdint.h>

#define MVAR_MAX_WRITERS 26
#define MVAR_MAX_READERS 14
#define MVAR_NAME_LEN 32

typedef struct {
    int context_id;
    int writer_count;
    int reader_count;
    int writer_pids[MVAR_MAX_WRITERS];
    int reader_pids[MVAR_MAX_READERS];
    char writer_names[MVAR_MAX_WRITERS][MVAR_NAME_LEN];
    char reader_names[MVAR_MAX_READERS][MVAR_NAME_LEN];
} mvar_launch_info_t;

int mvar_start(int writer_count, int reader_count, mvar_launch_info_t *out_info);

#endif /* MVAR_H */
