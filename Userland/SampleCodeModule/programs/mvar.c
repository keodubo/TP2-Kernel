#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys_calls.h>
#include <userlib.h>
#include "mvar.h"

#define MVAR_MAX_INSTANCES 4
#define NAME_LEN 32
#define SEM_MAX_NAME 32
#define MAX_WRITERS 26

typedef struct {
    int in_use;
    int id;
    volatile char value;
    char sem_empty_name[SEM_MAX_NAME];
    char sem_full_name[SEM_MAX_NAME];
} mvar_context_t;

typedef struct {
    const char *label;
    const Color *color;
} reader_profile_t;

static const reader_profile_t reader_palette[] = {
    {"red", &RED},
    {"green", &GREEN},
    {"blue", &BLUE},
    {"yellow", &YELLOW},
    {"cyan", &CYAN},
    {"orange", &ORANGE},
    {"purple", &PURPLE},
    {"pink", &PINK},
    {"lightgreen", &LIGHT_GREEN},
    {"lightblue", &LIGHT_BLUE},
    {"lightpurple", &LIGHT_PURPLE},
    {"lightorange", &LIGHT_ORANGE},
    {"lightyellow", &LIGHT_YELLOW},
    {"lightpink", &LIGHT_PINK}
};
static const int reader_palette_len = (int)(sizeof(reader_palette) / sizeof(reader_palette[0]));

static mvar_context_t contexts[MVAR_MAX_INSTANCES] = {0};
static int next_ctx_id = 1;

static uint32_t rng_step(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void active_random_wait(uint32_t *state) {
    uint32_t delay = (rng_step(state) % 60000) + 4000;
    for (volatile uint32_t i = 0; i < delay; i++) {
        if ((i & 0x3FFu) == 0) {
            sys_yield();
        }
    }
}

static mvar_context_t *ctx_lookup(int id) {
    for (int i = 0; i < MVAR_MAX_INSTANCES; i++) {
        if (contexts[i].in_use && contexts[i].id == id) {
            return &contexts[i];
        }
    }
    return NULL;
}

static mvar_context_t *ctx_allocate(void) {
    for (int i = 0; i < MVAR_MAX_INSTANCES; i++) {
        if (!contexts[i].in_use) {
            mvar_context_t *ctx = &contexts[i];
            ctx->in_use = 1;
            ctx->id = next_ctx_id++;
            ctx->value = 0;
            sprintf(ctx->sem_empty_name, "mvar_empty_%d", ctx->id);
            sprintf(ctx->sem_full_name, "mvar_full_%d", ctx->id);
            return ctx;
        }
    }
    return NULL;
}

static void writer_process(int argc, char **argv) {
    if (argc < 3) {
        sys_exit(-1);
        return;
    }

    int ctx_id = atoi(argv[0]);
    char letter = argv[1][0];
    int writer_idx = atoi(argv[2]);

    if (argv) {
        free(argv);
    }

    mvar_context_t *ctx = ctx_lookup(ctx_id);
    if (ctx == NULL) {
        sys_exit(-1);
        return;
    }

    int sem_empty = sys_sem_open(ctx->sem_empty_name, 1);
    int sem_full = sys_sem_open(ctx->sem_full_name, 0);
    if (sem_empty < 0 || sem_full < 0) {
        if (sem_empty >= 0) sys_sem_close(sem_empty);
        if (sem_full >= 0) sys_sem_close(sem_full);
        sys_exit(-1);
        return;
    }

    uint32_t rand_state = (uint32_t)sys_getpid() ^ ((uint32_t)writer_idx * 6971U);

    while (1) {
        active_random_wait(&rand_state);
        sys_sem_wait(sem_empty);
        ctx->value = letter;
        sys_sem_post(sem_full);
    }
}

static void reader_process(int argc, char **argv) {
    if (argc < 3) {
        sys_exit(-1);
        return;
    }

    int ctx_id = atoi(argv[0]);
    int reader_idx = atoi(argv[1]);
    int color_idx = atoi(argv[2]);

    if (argv) {
        free(argv);
    }

    mvar_context_t *ctx = ctx_lookup(ctx_id);
    if (ctx == NULL) {
        sys_exit(-1);
        return;
    }

    int sem_empty = sys_sem_open(ctx->sem_empty_name, 1);
    int sem_full = sys_sem_open(ctx->sem_full_name, 0);
    if (sem_empty < 0 || sem_full < 0) {
        if (sem_empty >= 0) sys_sem_close(sem_empty);
        if (sem_full >= 0) sys_sem_close(sem_full);
        sys_exit(-1);
        return;
    }

    Color color = *reader_palette[color_idx % reader_palette_len].color;

    uint32_t rand_state = (uint32_t)sys_getpid() ^ ((uint32_t)reader_idx * 9151U);

    while (1) {
        active_random_wait(&rand_state);
        sys_sem_wait(sem_full);
        char value = ctx->value;
        sys_sem_post(sem_empty);
        printcColor(value, color);
        sys_yield();
    }
}

int mvar_start(int writer_count, int reader_count, mvar_launch_info_t *out_info) {
    if (writer_count <= 0 || reader_count <= 0 ||
        writer_count > MVAR_MAX_WRITERS || reader_count > MVAR_MAX_READERS ||
        writer_count > MAX_WRITERS) {
        return -1;
    }

    mvar_context_t *ctx = ctx_allocate();
    if (ctx == NULL) {
        return -1;
    }

    int sem_empty = sys_sem_open(ctx->sem_empty_name, 1);
    int sem_full = sys_sem_open(ctx->sem_full_name, 0);
    if (sem_empty < 0 || sem_full < 0) {
        if (sem_empty >= 0) sys_sem_close(sem_empty);
        if (sem_full >= 0) sys_sem_close(sem_full);
        ctx->in_use = 0;
        return -1;
    }
    sys_sem_close(sem_empty);
    sys_sem_close(sem_full);

    if (out_info != NULL) {
        out_info->context_id = ctx->id;
        out_info->writer_count = writer_count;
        out_info->reader_count = reader_count;
        for (int i = 0; i < MVAR_MAX_WRITERS; i++) {
            out_info->writer_pids[i] = -1;
            out_info->writer_names[i][0] = '\0';
        }
        for (int i = 0; i < MVAR_MAX_READERS; i++) {
            out_info->reader_pids[i] = -1;
            out_info->reader_names[i][0] = '\0';
        }
    }

    for (int w = 0; w < writer_count; w++) {
        char **argv = malloc(sizeof(char *) * 4);
        if (argv == NULL) {
            return -1;
        }
        char *arg0 = malloc(16);
        char *arg1 = malloc(4);
        char *arg2 = malloc(16);
        if (!arg0 || !arg1 || !arg2) {
            if (arg0) free(arg0);
            if (arg1) free(arg1);
            if (arg2) free(arg2);
            free(argv);
            return -1;
        }
        sprintf(arg0, "%d", ctx->id);
        sprintf(arg1, "%c", 'A' + w);
        sprintf(arg2, "%d", w);

        argv[0] = arg0;
        argv[1] = arg1;
        argv[2] = arg2;
        argv[3] = NULL;

        char name[NAME_LEN];
        sprintf(name, "mvar-writer-%c", 'A' + w);

        int pid = sys_create_process_ex(writer_process, 3, argv, name, DEFAULT_PRIORITY, 0);
        if (pid < 0) {
            return -1;
        }
        if (out_info != NULL) {
            out_info->writer_pids[w] = pid;
            sprintf(out_info->writer_names[w], "mvar-writer-%c", 'A' + w);
        }
    }

    for (int r = 0; r < reader_count; r++) {
        char **argv = malloc(sizeof(char *) * 4);
        if (argv == NULL) {
            return -1;
        }
        char *arg0 = malloc(16);
        char *arg1 = malloc(16);
        char *arg2 = malloc(16);
        if (!arg0 || !arg1 || !arg2) {
            if (arg0) free(arg0);
            if (arg1) free(arg1);
            if (arg2) free(arg2);
            free(argv);
            return -1;
        }
        sprintf(arg0, "%d", ctx->id);
        sprintf(arg1, "%d", r);
        sprintf(arg2, "%d", r);

        argv[0] = arg0;
        argv[1] = arg1;
        argv[2] = arg2;
        argv[3] = NULL;

        const reader_profile_t *profile = &reader_palette[r % reader_palette_len];
        char name[NAME_LEN];
        sprintf(name, "mvar-reader-%s", profile->label);

        int pid = sys_create_process_ex(reader_process, 3, argv, name, DEFAULT_PRIORITY, 0);
        if (pid < 0) {
            return -1;
        }
        if (out_info != NULL) {
            out_info->reader_pids[r] = pid;
            sprintf(out_info->reader_names[r], "mvar-reader-%s", profile->label);
        }
    }

    return 0;
}
