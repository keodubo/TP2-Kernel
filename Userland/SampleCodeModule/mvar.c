#include <stdint.h>
#include <stdio.h>
#include <userlib.h>
#include <sys_calls.h>
#include "mvar.h"

// Simple MVar implementation following the specification:
// - Writers wait for empty, write value, signal full
// - Readers wait for full, read value, signal empty
// - Both do random active wait before each operation

#define MVAR_MAX_INSTANCES 4
#define SEM_NAME_LEN 32
#define NAME_LEN 32

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

typedef struct {
	int in_use;
	int id;
	volatile char value;  // The MVar value
	char sem_empty_name[SEM_NAME_LEN];
	char sem_full_name[SEM_NAME_LEN];
} mvar_context_t;

static mvar_context_t contexts[MVAR_MAX_INSTANCES] = {0};
static int next_context_id = 1;

// Simple xorshift PRNG for random delays
static uint32_t mvar_rand(uint32_t *state) {
	uint32_t x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*state = x;
	return x;
}

// Random active wait as per specification
static void mvar_random_wait(uint32_t *rand_state) {
	// Random delay between ~10K and ~500K iterations
	uint32_t delay = (mvar_rand(rand_state) % 490000) + 10000;
	for (volatile uint32_t i = 0; i < delay; i++) {
		// Active (busy) wait
	}
}

static mvar_context_t *mvar_get_context(int id) {
	for (int i = 0; i < MVAR_MAX_INSTANCES; i++) {
		if (contexts[i].in_use && contexts[i].id == id) {
			return &contexts[i];
		}
	}
	return NULL;
}

static mvar_context_t *mvar_allocate_context(void) {
	for (int i = 0; i < MVAR_MAX_INSTANCES; i++) {
		if (!contexts[i].in_use) {
			mvar_context_t *ctx = &contexts[i];
			ctx->in_use = 1;
			ctx->id = next_context_id++;
			ctx->value = 0;
			sprintf(ctx->sem_empty_name, "mvar_empty_%d", ctx->id);
			sprintf(ctx->sem_full_name, "mvar_full_%d", ctx->id);
			return ctx;
		}
	}
	return NULL;
}

// Writer process: performs random wait, waits for empty, writes, signals full
static void mvar_writer_process(int argc, char **argv) {
	if (argc < 3) {
		sys_exit(-1);
		return;
	}

	int context_id = atoi(argv[0]);
	int writer_idx = atoi(argv[1]);
	char letter = argv[2][0];

	// Clean up argv
	if (argv) {
		free(argv);
	}

	mvar_context_t *ctx = mvar_get_context(context_id);
	if (ctx == NULL) {
		sys_exit(-1);
		return;
	}

	// Open semaphores
	int sem_empty = (int)sys_sem_open(ctx->sem_empty_name, 1);
	int sem_full = (int)sys_sem_open(ctx->sem_full_name, 0);

	if (sem_empty < 0 || sem_full < 0) {
		if (sem_empty >= 0) sys_sem_close(sem_empty);
		if (sem_full >= 0) sys_sem_close(sem_full);
		sys_exit(-1);
		return;
	}

	// Initialize random seed with PID + index
	uint32_t rand_state = (uint32_t)sys_getpid() + (uint32_t)writer_idx * 7919;

	while (1) {
		// SPECIFICATION: "Each writer performs a random active wait"
		mvar_random_wait(&rand_state);

		// Wait for MVar to be empty (only one writer can get this at a time)
		sys_sem_wait(sem_empty);

		// Write the value
		ctx->value = letter;

		// Signal that MVar is full (allow one reader to proceed)
		sys_sem_post(sem_full);
	}
}

// Reader process: performs random wait, waits for full, reads, signals empty
static void mvar_reader_process(int argc, char **argv) {
	if (argc < 3) {
		sys_exit(-1);
		return;
	}

	int context_id = atoi(argv[0]);
	int reader_idx = atoi(argv[1]);
	int color_idx = atoi(argv[2]);

	// Clean up argv
	if (argv) {
		free(argv);
	}

	mvar_context_t *ctx = mvar_get_context(context_id);
	if (ctx == NULL) {
		sys_exit(-1);
		return;
	}

	// Open semaphores
	int sem_empty = (int)sys_sem_open(ctx->sem_empty_name, 1);
	int sem_full = (int)sys_sem_open(ctx->sem_full_name, 0);

	if (sem_empty < 0 || sem_full < 0) {
		if (sem_empty >= 0) sys_sem_close(sem_empty);
		if (sem_full >= 0) sys_sem_close(sem_full);
		sys_exit(-1);
		return;
	}

	const reader_profile_t *profile = &reader_palette[color_idx % reader_palette_len];
	Color color = *profile->color;

	// Initialize random seed with PID + index
	uint32_t rand_state = (uint32_t)sys_getpid() + (uint32_t)reader_idx * 7919;

	while (1) {
		// SPECIFICATION: "Each reader performs a random active wait"
		mvar_random_wait(&rand_state);

		// Wait for MVar to be full (only one reader can get this at a time)
		sys_sem_wait(sem_full);

		// Read and consume the value (critical section)
		char value = ctx->value;

		// Signal that MVar is empty (allow one writer to proceed)
		sys_sem_post(sem_empty);

		// Print AFTER releasing the lock (outside critical section)
		printcColor(value, color);

		// Add a busy wait to slow down output for better visibility
		for (volatile uint32_t i = 0; i < 5000000; i++) {
			// Busy wait
		}
	}
}

int mvar_start(int writer_count, int reader_count, mvar_launch_info_t *out_info) {
	if (writer_count <= 0 || reader_count <= 0 ||
	    writer_count > MVAR_MAX_WRITERS || reader_count > MVAR_MAX_READERS) {
		return -1;
	}

	mvar_context_t *ctx = mvar_allocate_context();
	if (ctx == NULL) {
		return -1;
	}

	// Pre-create semaphores in parent process
	int sem_empty = (int)sys_sem_open(ctx->sem_empty_name, 1);  // Start with 1 (empty)
	int sem_full = (int)sys_sem_open(ctx->sem_full_name, 0);   // Start with 0 (not full)

	if (sem_empty < 0 || sem_full < 0) {
		if (sem_empty >= 0) sys_sem_close(sem_empty);
		if (sem_full >= 0) sys_sem_close(sem_full);
		ctx->in_use = 0;
		return -1;
	}

	// Close parent's handles (children will open their own)
	sys_sem_close(sem_empty);
	sys_sem_close(sem_full);

	// Prepare output info
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

	// Spawn writers
	char context_id_str[16];
	sprintf(context_id_str, "%d", ctx->id);

	for (int w = 0; w < writer_count; w++) {
		char **argv = (char **)malloc(sizeof(char *) * 4);
		if (argv == NULL) {
			// TODO: cleanup spawned processes
			return -1;
		}

		char *arg0 = (char *)malloc(16);
		char *arg1 = (char *)malloc(16);
		char *arg2 = (char *)malloc(4);
		if (!arg0 || !arg1 || !arg2) {
			if (arg0) free(arg0);
			if (arg1) free(arg1);
			if (arg2) free(arg2);
			free(argv);
			return -1;
		}

		sprintf(arg0, "%d", ctx->id);
		sprintf(arg1, "%d", w);
		sprintf(arg2, "%c", 'A' + (w % 26));

		argv[0] = arg0;
		argv[1] = arg1;
		argv[2] = arg2;
		argv[3] = NULL;

		char name[NAME_LEN];
		sprintf(name, "mvar-writer-%c", 'A' + (w % 26));

		int pid = (int)sys_create_process_ex(mvar_writer_process, 3, argv, name, DEFAULT_PRIORITY, 0);

		if (pid < 0) {
			// TODO: cleanup
			return -1;
		}

		if (out_info != NULL) {
			out_info->writer_pids[w] = pid;
			sprintf(out_info->writer_names[w], "mvar-writer-%c", 'A' + (w % 26));
		}
	}

	// Spawn readers
	for (int r = 0; r < reader_count; r++) {
		char **argv = (char **)malloc(sizeof(char *) * 4);
		if (argv == NULL) {
			// TODO: cleanup spawned processes
			return -1;
		}

		char *arg0 = (char *)malloc(16);
		char *arg1 = (char *)malloc(16);
		char *arg2 = (char *)malloc(16);
		if (!arg0 || !arg1 || !arg2) {
			if (arg0) free(arg0);
			if (arg1) free(arg1);
			if (arg2) free(arg2);
			free(argv);
			return -1;
		}

		sprintf(arg0, "%d", ctx->id);
		sprintf(arg1, "%d", r);
		sprintf(arg2, "%d", r % reader_palette_len);

		argv[0] = arg0;
		argv[1] = arg1;
		argv[2] = arg2;
		argv[3] = NULL;

		const reader_profile_t *profile = &reader_palette[r % reader_palette_len];
		char name[NAME_LEN];
		sprintf(name, "mvar-reader-%s", profile->label);

		int pid = (int)sys_create_process_ex(mvar_reader_process, 3, argv, name, DEFAULT_PRIORITY, 0);

		if (pid < 0) {
			// TODO: cleanup
			return -1;
		}

		if (out_info != NULL) {
			out_info->reader_pids[r] = pid;
			sprintf(out_info->reader_names[r], "mvar-reader-%s", profile->label);
		}
	}

	// SPECIFICATION: "The main process must terminate immediately after creating readers and writers"
	// We don't wait or cleanup - just return

	return 0;
}

