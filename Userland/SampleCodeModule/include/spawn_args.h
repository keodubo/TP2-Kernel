#ifndef SPAWN_ARGS_H
#define SPAWN_ARGS_H

#include <sys_calls.h>

static inline void free_spawn_args(char **argv, int argc) {
	if (argv == NULL) {
		return;
	}
	for (int i = 0; i < argc; i++) {
		if (argv[i] != NULL) {
			sys_free(argv[i]);
		}
	}
	sys_free(argv);
}

#endif
