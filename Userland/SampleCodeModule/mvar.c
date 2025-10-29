#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <userlib.h>
#include <sys_calls.h>

#include "tests/syscall.h"

extern char parameter[];

typedef struct {
	volatile char slot;
	volatile int has_value;
} mvar_cell_t;

typedef struct {
	mvar_cell_t cell;
	char sem_empty[32];
	char sem_full[32];
	char sem_mutex[32];
	int writer_count;
	int reader_count;
	uint64_t instance_id;
} mvar_shared_state_t;

static void mvar_writer_entry(int argc, char **argv);
static void mvar_reader_entry(int argc, char **argv);
static void busy_wait_random(uint32_t salt);
static int spawn_mvar_writer(mvar_shared_state_t *shared, int index);
static int spawn_mvar_reader(mvar_shared_state_t *shared, int index);
static char *alloc_string_from_value(uint64_t value);
static void free_spawn_args_local(char **argv, int argc);
static int parse_positive_count(const char *token, int *out_value);
static int parse_mvar_args(const char *input, int *writers, int *readers);
static Color pick_reader_color(int index);

void cmd_mvar(void) {
	int writers = 0;
	int readers = 0;

	if (!parse_mvar_args(parameter, &writers, &readers)) {
		printf("Usage: mvar <writers> <readers>\n");
		return;
	}

	mvar_shared_state_t *shared = (mvar_shared_state_t *)sys_malloc(sizeof(mvar_shared_state_t));
	if (shared == NULL) {
		printf("[mvar] ERROR: unable to allocate shared state\n");
		return;
	}

	memset(shared, 0, sizeof(*shared));
	shared->cell.slot = 0;
	shared->cell.has_value = 0;
	shared->writer_count = writers;
	shared->reader_count = readers;

	static uint32_t mvar_sequence = 0;
	uint32_t seq = ++mvar_sequence;
	int64_t caller_pid = sys_getpid();

	sprintf(shared->sem_empty, "mvarE_%ld_%u", (long)caller_pid, seq);
	sprintf(shared->sem_full, "mvarF_%ld_%u", (long)caller_pid, seq);
	sprintf(shared->sem_mutex, "mvarM_%ld_%u", (long)caller_pid, seq);
	shared->instance_id = ((uint64_t)caller_pid << 32) ^ seq;

	// Inicializar los semáforos ANTES de crear los procesos hijos
	// Esto asegura que el primer proceso que los use tenga los valores correctos
	// sem_empty = 1 (MVar vacío, puede escribir)
	// sem_full = 0 (MVar vacío, no puede leer)
	// sem_mutex = 1 (desbloqueado)
	// Los hijos abrirán los semáforos por nombre y compartirán las mismas instancias del kernel
	// NO cerramos los semáforos - los dejamos abiertos para que los hijos puedan encontrarlos
	// Los semáforos persistirán hasta que todos los procesos (incluido el padre) terminen
	int tmp_empty = my_sem_open(shared->sem_empty, 1);
	int tmp_full = my_sem_open(shared->sem_full, 0);
	int tmp_mutex = my_sem_open(shared->sem_mutex, 1);
	
	if (tmp_empty < 0 || tmp_full < 0 || tmp_mutex < 0) {
		printf("[mvar] ERROR: unable to initialize semaphores\n");
		sys_free(shared);
		return;
	}
	
	// No cerramos los semáforos aquí - los hijos los abrirán por nombre y encontrarán
	// las instancias existentes del kernel. Cuando el proceso principal termine,
	// los handles locales se limpiarán automáticamente.

	printf("[mvar] Launching %d writer(s) and %d reader(s)\n", writers, readers);

	int error = 0;

	for (int i = 0; i < writers; i++) {
		if (spawn_mvar_writer(shared, i) < 0) {
			error = 1;
			break;
		}
	}

	if (!error) {
		for (int i = 0; i < readers; i++) {
			if (spawn_mvar_reader(shared, i) < 0) {
				error = 1;
				break;
			}
		}
	}

	if (error) {
		printf("[mvar] WARNING: failed to launch all participants. Some processes may still be running.\n");
	} else {
		printf("[mvar] Processes running in background. Use ps/kill/nice as needed.\n");
	}
}

static const char *reader_name_for_index(int index) {
    static const char *names[] = {"r-red","r-grn","r-blu","r-yel","r-cyn","r-mag"};
    int n = (int)(sizeof(names)/sizeof(names[0]));
    if (index >= 0 && index < n) return names[index];
    return NULL; // llamador puede generar r-<idx>
}

static int spawn_mvar_writer(mvar_shared_state_t *shared, int index) {
    char **argv = (char **)sys_malloc(sizeof(char *) * 4);
	if (argv == NULL) {
		printf("[mvar] ERROR: unable to allocate argv for writer %d\n", index);
		return -1;
	}

	argv[0] = alloc_string_from_value((uint64_t)(uintptr_t)shared);
	argv[1] = alloc_string_from_value((uint64_t)index);
    // argv[2] = nombre lógico del proceso para que el hijo lo imprima
    char pname[8];
    char letter = (char)('A' + (index % 26));
    sprintf(pname, "w-%c", letter);
    argv[2] = (char *)sys_malloc(strlen(pname) + 1);
    if (argv[2] != NULL) {
        strcpy(argv[2], pname);
    }
    argv[3] = NULL;

    if (argv[0] == NULL || argv[1] == NULL || argv[2] == NULL) {
        free_spawn_args_local(argv, 3);
		sys_free(argv);
		printf("[mvar] ERROR: unable to prepare arguments for writer %d\n", index);
		return -1;
	}

    char name[32];
    sprintf(name, "w-%c", letter);

    // CORRECCIÓN STARDVATION: Todos los escritores usan la misma prioridad (DEFAULT_PRIORITY)
    // para evitar starvation. Si se quiere que un escritor aparezca más frecuentemente que otro,
    // se debe hacer ajustando el busy_wait_random, no con prioridades diferentes.
    // El scheduler estricto por prioridad causaba que escritores con menor prioridad nunca ejecutaran.
    uint8_t prio = DEFAULT_PRIORITY;

    int64_t pid = sys_create_process_ex(mvar_writer_entry, 3, argv, name, prio, 0);
	if (pid < 0) {
        free_spawn_args_local(argv, 3);
		sys_free(argv);
		printf("[mvar] ERROR: failed to spawn writer %d\n", index);
		return -1;
	}

	printf("[mvar] writer[%d] -> PID %ld (letter %c)\n", index, (long)pid, letter);
	return 0;
}

static int spawn_mvar_reader(mvar_shared_state_t *shared, int index) {
    char **argv = (char **)sys_malloc(sizeof(char *) * 4);
	if (argv == NULL) {
		printf("[mvar] ERROR: unable to allocate argv for reader %d\n", index);
		return -1;
	}

	argv[0] = alloc_string_from_value((uint64_t)(uintptr_t)shared);
	argv[1] = alloc_string_from_value((uint64_t)index);
    const char *rname = reader_name_for_index(index);
    char buf[16];
    if (rname == NULL) {
        sprintf(buf, "r-%d", index);
        rname = buf;
    }
    argv[2] = (char *)sys_malloc(strlen(rname) + 1);
    if (argv[2] != NULL) {
        strcpy(argv[2], rname);
    }
    argv[3] = NULL;

    if (argv[0] == NULL || argv[1] == NULL || argv[2] == NULL) {
        free_spawn_args_local(argv, 3);
		sys_free(argv);
		printf("[mvar] ERROR: unable to prepare arguments for reader %d\n", index);
		return -1;
	}

    char name[32];
    sprintf(name, "%s", argv[2]);

    // Lectores usan MAX_PRIORITY cuando hay menos lectores que escritores
    // para consumir rápidamente y dar más oportunidades a todos los escritores
    // (que tienen la misma prioridad entre sí)
    uint8_t reader_prio = DEFAULT_PRIORITY;
    if (shared->writer_count > shared->reader_count) {
        reader_prio = MAX_PRIORITY;
    }

    int64_t pid = sys_create_process_ex(mvar_reader_entry, 3, argv, name, reader_prio, 0);
	if (pid < 0) {
        free_spawn_args_local(argv, 3);
		sys_free(argv);
		printf("[mvar] ERROR: failed to spawn reader %d\n", index);
		return -1;
	}

	printf("[mvar] reader[%d] -> PID %ld\n", index, (long)pid);
	return 0;
}

static char *alloc_string_from_value(uint64_t value) {
	char buffer[32];
	sprintf(buffer, "%ld", (int64_t)value);
	size_t len = strlen(buffer) + 1;
	char *out = (char *)sys_malloc(len);
	if (out == NULL) {
		return NULL;
	}
	strcpy(out, buffer);
	return out;
}

static void free_spawn_args_local(char **argv, int argc) {
	if (argv == NULL) {
		return;
	}
	for (int i = 0; i < argc; i++) {
		if (argv[i] != NULL) {
			sys_free(argv[i]);
		}
	}
}

static int parse_positive_count(const char *token, int *out_value) {
	if (token == NULL || out_value == NULL || token[0] == '\0') {
		return 0;
	}
	int value = 0;
	for (int i = 0; token[i] != '\0'; i++) {
		if (token[i] < '0' || token[i] > '9') {
			return 0;
		}
		value = value * 10 + (token[i] - '0');
		if (value > 999999) {
			return 0;
		}
	}
	if (value < 1) {
		return 0;
	}
	*out_value = value;
	return 1;
}

static int parse_mvar_args(const char *input, int *writers, int *readers) {
	if (input == NULL) {
		return 0;
	}

	char token0[32] = {0};
	char token1[32] = {0};

	int idx = 0;
	int t0 = 0;
	while (input[idx] == ' ') {
		idx++;
	}
	while (input[idx] != '\0' && input[idx] != ' ') {
		if (t0 < (int)(sizeof(token0) - 1)) {
			token0[t0++] = input[idx];
		}
		idx++;
	}
	token0[t0] = '\0';

	while (input[idx] == ' ') {
		idx++;
	}

	int t1 = 0;
	while (input[idx] != '\0' && input[idx] != ' ') {
		if (t1 < (int)(sizeof(token1) - 1)) {
			token1[t1++] = input[idx];
		}
		idx++;
	}
	token1[t1] = '\0';

	while (input[idx] == ' ') {
		idx++;
	}
	if (input[idx] != '\0') {
		return 0;
	}

	if (!parse_positive_count(token0, writers)) {
		return 0;
	}
	if (!parse_positive_count(token1, readers)) {
		return 0;
	}
	return 1;
}

static void busy_wait_random(uint32_t salt) {
	uint32_t state = (uint32_t)(my_getpid() ^ salt ^ 0x9E3779B9u);
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	uint32_t spins = 2000u + (state & 0x3FFFu);
	for (uint32_t i = 0; i < spins; i++) {
		if ((i & 0x3Fu) == 0) {
			my_yield();
		}
	}
}

static void mvar_writer_entry(int argc, char **argv) {
    if (argc < 3 || argv == NULL) {
		sys_exit(-1);
		return;
	}

	mvar_shared_state_t *shared = (mvar_shared_state_t *)(uintptr_t)charToInt(argv[0]);
    int index = atoi(argv[1]);
    const char *selfname = (argv[2] != NULL) ? argv[2] : "w";

	free_spawn_args_local(argv, argc);
	sys_free(argv);

    if (shared == NULL) {
		sys_exit(-1);
		return;
	}

	if (my_sem_open(shared->sem_empty, 1) < 0 ||
		my_sem_open(shared->sem_full, 0) < 0 ||
		my_sem_open(shared->sem_mutex, 1) < 0) {
		printf("[mvar-w%d] semaphore setup failed\n", index);
		sys_exit(-1);
		return;
	}

    printf("[mvar] %s pid=%ld\n", selfname, (long)my_getpid());

    while (1) {
		busy_wait_random((uint32_t)(shared->instance_id + (uint32_t)index * 37u));
		my_sem_wait(shared->sem_empty);
		my_sem_wait(shared->sem_mutex);

		char letter = (char)('A' + (index % 26));
		shared->cell.slot = letter;
		shared->cell.has_value = 1;

		my_sem_post(shared->sem_mutex);
		my_sem_post(shared->sem_full);
	}
}

static void mvar_reader_entry(int argc, char **argv) {
    if (argc < 3 || argv == NULL) {
		sys_exit(-1);
		return;
	}

	mvar_shared_state_t *shared = (mvar_shared_state_t *)(uintptr_t)charToInt(argv[0]);
    int index = atoi(argv[1]);
    const char *selfname = (argv[2] != NULL) ? argv[2] : "reader";

	free_spawn_args_local(argv, argc);
	sys_free(argv);

	if (shared == NULL) {
		sys_exit(-1);
		return;
	}

	if (my_sem_open(shared->sem_empty, 1) < 0 ||
		my_sem_open(shared->sem_full, 0) < 0 ||
		my_sem_open(shared->sem_mutex, 1) < 0) {
		printf("[mvar-r%d] semaphore setup failed\n", index);
		sys_exit(-1);
		return;
	}

    Color color = pick_reader_color(index);
    printf("[mvar] %s pid=%ld\n", selfname, (long)my_getpid());

	while (1) {
		busy_wait_random((uint32_t)(shared->instance_id + (uint32_t)index * 131u));
		my_sem_wait(shared->sem_full);
		my_sem_wait(shared->sem_mutex);

		char value = shared->cell.slot;
		shared->cell.has_value = 0;

		printcColor(value, color);

		my_sem_post(shared->sem_mutex);
		my_sem_post(shared->sem_empty);
	}
}

static Color pick_reader_color(int index) {
	static const Color *palette[] = {
		&LIGHT_RED,
		&LIGHT_GREEN,
		&LIGHT_BLUE,
		&LIGHT_ORANGE,
		&LIGHT_PURPLE,
		&LIGHT_YELLOW,
		&CYAN,
		&ORANGE,
		&GREEN,
		&PINK,
		&PURPLE,
		&YELLOW,
		&BLUE
	};
	const int palette_size = (int)(sizeof(palette) / sizeof(palette[0]));
	if (palette_size <= 0) {
		Color fallback = {255, 255, 255};
		return fallback;
	}
	const Color *selected = palette[index % palette_size];
	if (selected == NULL) {
		Color fallback = {255, 255, 255};
		return fallback;
	}
	return *selected;
}
