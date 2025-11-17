#include <stdio.h>
#include <stdint.h>
#include <sys_calls.h>
#include <spawn_args.h>
#include <userlib.h>
#include <test_util.h>

#define MIN_PHILOS 1
#define MAX_PHILOS 10
#define INITIAL_PHILO 5

#define STATE_THINKING 0
#define STATE_EATING 1

#define MUTEX_PHILOSOPHERS "philo_mutex_state"
#define MUTEX_COUNT_PHILO "philo_mutex_count"
#define WAITER_SEM "philo_waiter"

typedef struct {
	char *sem_name;
	int fork_sem;
	int pid;
	int hunger;
	int state;
} philosopher_t;

static philosopher_t philosophers[MAX_PHILOS];
static char fork_name_storage[MAX_PHILOS][32];
static int mutex_philosophers = -1;
static int mutex_count = -1;
static int waiter_sem = -1;
static volatile int count_philo = 0;

static void print_state(void);
static void eat(int id);
static void think(int id);
static void think_idle(int id);
static void philosopher_entry(int argc, char **argv);
static int init_philosopher_slot(int idx);
static int init_all_philosophers(void);
static char **build_philo_args(int id, int *argc_out);
static void shutdown_philosophers_and_print_stats(void);
static void terminate_all_children(void);
static void release_fork(int idx);
static void cleanup_global_sync(void);
static void sleep_random(int id);
static void snapshot_state_async(void);
static void reset_philosophers(void);

static void sleep_random(int id) {
	uint32_t base = 40 + (uint32_t)(id * 17);
	uint32_t jitter = GetUniform(400) + base;
	sys_wait((uint64_t)(jitter + 60));
}

static void print_state(void) {
	sys_sem_wait(mutex_count);
	int local_count = count_philo;
	sys_sem_post(mutex_count);

	if (local_count <= 0) {
		return;
	}

	for (int i = 0; i < local_count; i++) {
		if (philosophers[i].state == STATE_EATING) {
			prints("E ", 3);
		} else {
			prints(". ", 3);
		}
	}
	printc('\n');
}

static void eat(int id) {
	sys_sem_wait(mutex_philosophers);
	philosophers[id].state = STATE_EATING;
	sys_sem_wait(mutex_count);
	if (id < count_philo) {
		philosophers[id].hunger++;
	}
	sys_sem_post(mutex_count);
	print_state();
	sys_sem_post(mutex_philosophers);
	sleep_random(id);
}

static void think(int id) {
	sys_sem_wait(mutex_philosophers);
	philosophers[id].state = STATE_THINKING;
	print_state();
	sys_sem_post(mutex_philosophers);
	sleep_random(id);
}

static void think_idle(int id) {
	if (mutex_philosophers >= 0) {
		sys_sem_wait(mutex_philosophers);
		philosophers[id].state = STATE_THINKING;
		sys_sem_post(mutex_philosophers);
	}
	sys_wait(150);
	sys_yield();
}

static char **build_philo_args(int id, int *argc_out) {
	char **argv = (char **)sys_malloc(sizeof(char *) * 2);
	if (argv == NULL) {
		return NULL;
	}
	argv[1] = NULL;

	argv[0] = (char *)sys_malloc(16);
	if (argv[0] == NULL) {
		sys_free(argv);
		return NULL;
	}

	sprintf(argv[0], "%d", id);
	if (argc_out != NULL) {
		*argc_out = 1;
	}
	return argv;
}

static int init_philosopher_slot(int idx) {
	sprintf(fork_name_storage[idx], "philo_fork_%d", idx);
	philosophers[idx].sem_name = fork_name_storage[idx];

	philosophers[idx].fork_sem = sys_sem_open(philosophers[idx].sem_name, 1);
	if (philosophers[idx].fork_sem < 0) {
		return -1;
	}

	int argc_spawn = 0;
	char **argv = build_philo_args(idx, &argc_spawn);
	if (argv == NULL) {
		release_fork(idx);
		return -1;
	}

	char proc_name[32];
	sprintf(proc_name, "phylo_worker_%d", idx);

	int64_t pid = sys_create_process_ex(philosopher_entry, argc_spawn, argv, proc_name, DEFAULT_PRIORITY, 0);
	if (pid < 0) {
		free_spawn_args(argv, argc_spawn);
		release_fork(idx);
		return -1;
	}

	philosophers[idx].pid = (int)pid;
	philosophers[idx].state = STATE_THINKING;
	philosophers[idx].hunger = 0;
	return 0;
}

static int init_all_philosophers(void) {
	for (int i = 0; i < MAX_PHILOS; i++) {
		if (init_philosopher_slot(i) < 0) {
			return -1;
		}
	}
	return 0;
}

static void release_fork(int idx) {
	if (philosophers[idx].fork_sem >= 0 && philosophers[idx].sem_name != NULL) {
		sys_sem_close(philosophers[idx].fork_sem);
		sys_sem_unlink(philosophers[idx].sem_name);
		philosophers[idx].fork_sem = -1;
		philosophers[idx].sem_name = NULL;
	}
}

static void cleanup_global_sync(void) {
	if (mutex_philosophers >= 0) {
		sys_sem_close(mutex_philosophers);
		sys_sem_unlink(MUTEX_PHILOSOPHERS);
		mutex_philosophers = -1;
	}
	if (mutex_count >= 0) {
		sys_sem_close(mutex_count);
		sys_sem_unlink(MUTEX_COUNT_PHILO);
		mutex_count = -1;
	}
	if (waiter_sem >= 0) {
		sys_sem_close(waiter_sem);
		sys_sem_unlink(WAITER_SEM);
		waiter_sem = -1;
	}
}

static void terminate_all_children(void) {
	sys_sem_wait(mutex_count);
	count_philo = 0;
	sys_sem_post(mutex_count);

	for (int i = 0; i < MAX_PHILOS; i++) {
		if (philosophers[i].pid > 0) {
			sys_kill(philosophers[i].pid);
			int status = 0;
			sys_wait_pid(philosophers[i].pid, &status);
			philosophers[i].pid = -1;
		}
	}
}

static void snapshot_state_async(void) {
	if (mutex_philosophers >= 0) {
		sys_sem_wait(mutex_philosophers);
		print_state();
		sys_sem_post(mutex_philosophers);
	}
}

static void reset_philosophers(void) {
	for (int i = 0; i < MAX_PHILOS; i++) {
		philosophers[i].sem_name = NULL;
		philosophers[i].fork_sem = -1;
		philosophers[i].pid = -1;
		philosophers[i].hunger = 0;
		philosophers[i].state = STATE_THINKING;
		fork_name_storage[i][0] = '\0';
	}
}

static void philosopher_entry(int argc, char **argv) {
	int id = 0;
	if (argc >= 1 && argv != NULL && argv[0] != NULL) {
		id = (int)satoi(argv[0]);
	}
	free_spawn_args(argv, argc);

	while (1) {
		sys_sem_wait(mutex_count);
		int local_count = count_philo;
		sys_sem_post(mutex_count);

		if (local_count == 0) {
			break;
		}
		if (id >= local_count) {
			think_idle(id);
			continue;
		}

		int right = (id + 1) % local_count;

		sys_sem_wait(waiter_sem);
		sys_sem_wait(philosophers[id].fork_sem);
		int same_fork = (right == id);
		if (!same_fork) {
			sys_sem_wait(philosophers[right].fork_sem);
		}

		eat(id);

		if (!same_fork) {
			sys_sem_post(philosophers[right].fork_sem);
		}
		sys_sem_post(philosophers[id].fork_sem);
		sys_sem_post(waiter_sem);

		think(id);
	}

	sys_exit(0);
}

static void shutdown_philosophers_and_print_stats(void) {
	sys_sem_wait(mutex_count);
	count_philo = 0;
	sys_sem_post(mutex_count);

	for (int i = 0; i < MAX_PHILOS; i++) {
		if (philosophers[i].pid > 0) {
			int status = 0;
			sys_wait_pid(philosophers[i].pid, &status);
			philosophers[i].pid = -1;
		}
	}

	printf("\n");
	for (int i = 0; i < MAX_PHILOS; i++) {
		printf("Philosopher %d ate %d times\n", i, philosophers[i].hunger);
	}
}

void phylo_main(int argc, char **argv) {
	printf("PHILOSOPHER SIMULATION STARTED\n");
	printf("Press 'a' to add a philosopher, 'r' to remove one or 'x' to exit\n");

	if (INITIAL_PHILO < MIN_PHILOS || INITIAL_PHILO > MAX_PHILOS) {
		count_philo = MIN_PHILOS;
	} else {
		count_philo = INITIAL_PHILO;
	}
	reset_philosophers();

	mutex_philosophers = sys_sem_open(MUTEX_PHILOSOPHERS, 1);
	mutex_count = sys_sem_open(MUTEX_COUNT_PHILO, 1);
	waiter_sem = sys_sem_open(WAITER_SEM, MAX_PHILOS - 1);

	if (mutex_philosophers < 0 || mutex_count < 0 || waiter_sem < 0) {
		printf("[phylo] Failed to create synchronization primitives\n");
		cleanup_global_sync();
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	if (init_all_philosophers() < 0) {
		printf("[phylo] Failed to start philosopher processes\n");
		terminate_all_children();
		for (int i = 0; i < MAX_PHILOS; i++) {
			release_fork(i);
		}
		cleanup_global_sync();
		free_spawn_args(argv, argc);
		sys_exit(1);
	}

	snapshot_state_async();

	char c;
	while ((c = getChar()) != 'x') {
		if (c == 'a') {
			sys_sem_wait(mutex_count);
			if (count_philo < MAX_PHILOS) {
				count_philo++;
				printf("Added philosopher (total: %d)\n", count_philo);
			} else {
				printf("Cannot add more philosophers (max %d)\n", MAX_PHILOS);
			}
			sys_sem_post(mutex_count);
			snapshot_state_async();
		} else if (c == 'r') {
			sys_sem_wait(mutex_count);
			if (count_philo > MIN_PHILOS) {
				count_philo--;
				printf("Removed philosopher (total: %d)\n", count_philo);
			} else {
				printf("Cannot go below %d philosopher\n", MIN_PHILOS);
			}
			sys_sem_post(mutex_count);
			snapshot_state_async();
		}
	}

	shutdown_philosophers_and_print_stats();
	printf("PHILOSOPHER SIMULATION FINISHED\n");

	for (int i = 0; i < MAX_PHILOS; i++) {
		release_fork(i);
	}

	cleanup_global_sync();
	free_spawn_args(argv, argc);
	sys_exit(0);
}
