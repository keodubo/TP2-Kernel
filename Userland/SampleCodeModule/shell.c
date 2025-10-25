#include <stdio.h>
#include <sys_calls.h>
#include <userlib.h>
#include "sh/jobs.h"

/**
 * @file shell.c
 * @brief Shell minimalista con soporte para foreground/background
 * 
 * Funcionalidad:
 * - Ejecuta comandos básicos
 * - Soporte para & al final (background)
 * - Comandos integrados: jobs, fg, help, exit
 */

#define MAX_LINE 256
#define MAX_ARGS 32

// Prototipos de comandos integrados
static int cmd_jobs(int argc, char **argv);
static int cmd_fg(int argc, char **argv);
static int cmd_help(int argc, char **argv);
static int cmd_exit(int argc, char **argv);
static int cmd_ps(int argc, char **argv);
static int cmd_kill(int argc, char **argv);
static int cmd_nice(int argc, char **argv);

// Funciones auxiliares externas (declaradas en otros módulos)
extern int64_t test_processes(uint64_t argc, char *argv[]);
extern int64_t test_prio(uint64_t argc, char *argv[]);
extern int64_t test_sync(uint64_t argc, char *argv[]);
extern uint64_t test_mm(uint64_t argc, char *argv[]);
extern uint64_t test_no_synchro(uint64_t argc, char *argv[]);
extern uint64_t test_synchro(uint64_t argc, char *argv[]);
extern void cat_main(int argc, char **argv);
extern void wc_main(int argc, char **argv);
extern void filter_main(int argc, char **argv);
extern void loop_main(int argc, char **argv);
extern void mem_main(int argc, char **argv);

// Wrappers para tests con firma diferente
static void test_processes_wrapper(int argc, char **argv) {
    char *default_arg = "10";
    char **args = (argc > 0) ? argv : &default_arg;
    uint64_t count = (argc > 0) ? argc : 1;
    test_processes(count, args);
}

static void test_prio_wrapper(int argc, char **argv) {
    char *default_arg = "1000000";
    char **args = (argc > 0) ? argv : &default_arg;
    uint64_t count = (argc > 0) ? argc : 1;
    test_prio(count, args);
}

static void test_sync_wrapper(int argc, char **argv) {
    char *default_args[] = {"1", "0"};
    char **args = (argc >= 2) ? argv : default_args;
    uint64_t count = (argc >= 2) ? argc : 2;
    test_sync(count, args);
}

static void test_mm_wrapper(int argc, char **argv) {
    char *default_arg = "1024";
    char **args = (argc > 0) ? argv : &default_arg;
    uint64_t count = (argc > 0) ? argc : 1;
    test_mm(count, args);
}

static void test_no_synchro_wrapper(int argc, char **argv) {
    char *default_arg = "10";
    char **args = (argc > 0) ? argv : &default_arg;
    uint64_t count = (argc > 0) ? argc : 1;
    test_no_synchro(count, args);
}

static void test_synchro_wrapper(int argc, char **argv) {
    char *default_args[] = {"10", "1"};
    char **args = (argc >= 2) ? argv : default_args;
    uint64_t count = (argc >= 2) ? argc : 2;
    test_synchro(count, args);
}

static char **clone_args(char **argv, int count) {
    if (argv == NULL || count <= 0) {
        return NULL;
    }

    char **copy = (char **)sys_malloc(sizeof(char *) * (count + 1));
    if (copy == NULL) {
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        const char *src = argv[i];
        if (src == NULL) {
            copy[i] = NULL;
            continue;
        }
        int len = strlen(src);
        char *dst = (char *)sys_malloc((uint64_t)(len + 1));
        if (dst == NULL) {
            for (int j = 0; j < i; j++) {
                if (copy[j] != NULL) {
                    sys_free(copy[j]);
                }
            }
            sys_free(copy);
            return NULL;
        }
        for (int j = 0; j <= len; j++) {
            dst[j] = src[j];
        }
        copy[i] = dst;
    }
    copy[count] = NULL;
    return copy;
}

static void free_cloned_args(char **argv, int count) {
    if (argv == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        if (argv[i] != NULL) {
            sys_free(argv[i]);
        }
    }
    sys_free(argv);
}

// Tabla de comandos integrados
typedef struct {
    const char *name;
    int (*func)(int, char **);
    const char *help;
} builtin_cmd_t;

static const builtin_cmd_t builtins[] = {
    {"jobs", cmd_jobs, "List background jobs"},
    {"fg", cmd_fg, "Bring a job to foreground: fg <pid>"},
    {"help", cmd_help, "Show this help"},
    {"exit", cmd_exit, "Exit the shell"},
    {"ps", cmd_ps, "List all processes"},
    {"kill", cmd_kill, "Kill a process: kill <pid>"},
    {"nice", cmd_nice, "Change priority: nice <pid> <prio>"},
    {NULL, NULL, NULL}
};

// Función de ayuda
static int cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    printf("Available built-in commands:\n");
    for (int i = 0; builtins[i].name != NULL; i++) {
        printf("  %s - %s\n", builtins[i].name, builtins[i].help);
    }
    
    printf("\nAvailable programs:\n");
    printf("  test_processes - Test process management\n");
    printf("  test_prio - Test priority scheduling\n");
    printf("  test_sync - Test synchronization\n");
    printf("  test_mm - Test memory management\n");
    printf("  cat - Concatenate and print files\n");
    printf("  wc - Word count\n");
    printf("  filter - Filter vowels\n");
    printf("  loop - Infinite loop\n");
    printf("  mem [-v] - Show memory usage\n");

    printf("\nUse '&' at the end to run in background\n");
    printf("Example: loop &\n");
    
    return 0;
}

static int cmd_jobs(int argc, char **argv) {
    (void)argc;
    (void)argv;
    jobs_list();
    return 0;
}

static int cmd_fg(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: fg <pid>\n");
        return -1;
    }
    
    int pid = atoi(argv[1]);
    if (pid <= 0) {
        printf("Invalid PID\n");
        return -1;
    }
    
    // Verificar que el job existe
    if (!jobs_has(pid)) {
        printf("Job %d not found\n", pid);
        return -1;
    }
    
    // TODO: Implementar señal SIGCONT si es necesario
    // Por ahora solo esperamos al proceso
    printf("[fg] %d\n", pid);
    
    int status = 0;
    int waited = sys_wait_pid(pid, &status);
    if (waited > 0) {
        jobs_remove(pid);
        printf("[fg] %d exited with status %d\n", waited, status);
    }
    
    return 0;
}

static int cmd_exit(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("Exiting shell...\n");
    sys_exit(0);
    return 0;
}

static int cmd_ps(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    proc_info_t procs[64];
    int64_t count = sys_proc_snapshot(procs, 64);
    
    printf("PID   PRIO  STATE  TICKS  FG  NAME\n");
    printf("----  ----  -----  -----  --  ----\n");
    
    for (int i = 0; i < count; i++) {
        const char *state_str = "?????";
        switch (procs[i].state) {
            case 0: state_str = "NEW  "; break;
            case 1: state_str = "READY"; break;
            case 2: state_str = "RUN  "; break;
            case 3: state_str = "BLOCK"; break;
            case 4: state_str = "EXIT "; break;
        }
        
        printf("%4d  %4d  %s  %5d  %2s  %s\n",
               procs[i].pid,
               procs[i].priority,
               state_str,
               procs[i].ticks_left,
               procs[i].fg ? "Y" : "N",
               procs[i].name);
    }
    
    return 0;
}

static int cmd_kill(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: kill <pid>\n");
        return -1;
    }
    
    int pid = atoi(argv[1]);
    if (pid <= 0) {
        printf("Invalid PID\n");
        return -1;
    }
    
    int64_t result = sys_kill(pid);
    if (result < 0) {
        printf("Failed to kill process %d\n", pid);
        return -1;
    }
    
    printf("Killed process %d\n", pid);
    return 0;
}

static int cmd_nice(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: nice <pid> <priority>\n");
        return -1;
    }
    
    int pid = atoi(argv[1]);
    int prio = atoi(argv[2]);
    
    if (pid <= 0 || prio < 0 || prio > 3) {
        printf("Invalid arguments (prio must be 0-3)\n");
        return -1;
    }
    
    sys_nice(pid, prio);
    printf("Changed priority of process %d to %d\n", pid, prio);
    
    return 0;
}

// Tokeniza una línea en argumentos
static int tokenize(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;
    
    while (*p != '\0' && argc < max_args - 1) {
        // Saltar espacios
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        
        if (*p == '\0') {
            break;
        }
        
        argv[argc++] = p;
        
        // Buscar fin de palabra
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }
        
        if (*p != '\0') {
            *p = '\0';
            p++;
        }
    }
    
    argv[argc] = NULL;
    return argc;
}

// Lanza un proceso externo
static int launch_process(const char *name, char **argv, int is_background) {
    void (*entry)(int, char **) = NULL;
    
    // Mapeo de nombres a funciones
    if (strcmp(name, "test_processes") == 0) {
        entry = test_processes_wrapper;
    } else if (strcmp(name, "test_prio") == 0) {
        entry = test_prio_wrapper;
    } else if (strcmp(name, "test_sync") == 0) {
        entry = test_sync_wrapper;
    } else if (strcmp(name, "test_mm") == 0) {
        entry = test_mm_wrapper;
    } else if (strcmp(name, "cat") == 0) {
        entry = cat_main;
    } else if (strcmp(name, "wc") == 0) {
        entry = wc_main;
    } else if (strcmp(name, "filter") == 0) {
        entry = filter_main;
    } else if (strcmp(name, "loop") == 0) {
        entry = loop_main;
    } else if (strcmp(name, "test_no_synchro") == 0) {
        entry = test_no_synchro_wrapper;
    } else if (strcmp(name, "test_synchro") == 0) {
        entry = test_synchro_wrapper;
    } else if (strcmp(name, "test_fg_bg") == 0) {
        extern void test_fg_bg_main(int, char **);
        entry = test_fg_bg_main;
    } else if (strcmp(name, "mem") == 0) {
        entry = mem_main;
    } else {
        printf("Unknown command: %s\n", name);
        return -1;
    }

    int argc_total = 0;
    if (argv != NULL) {
        while (argv[argc_total] != NULL) {
            argc_total++;
        }
    }

    int arg_count = (argc_total > 1) ? (argc_total - 1) : 0;
    char **argv_copy = NULL;
    if (arg_count > 0) {
        argv_copy = clone_args(&argv[1], arg_count);
        if (argv_copy == NULL) {
            printf("Failed to allocate arguments\n");
            return -1;
        }
    }

    // Crear el proceso como background o foreground
    // Usar la nueva syscall que permite especificar el flag fg/bg
    int is_fg = is_background ? 0 : 1;
    int64_t pid = sys_create_process_ex(entry, arg_count, argv_copy, name, 2, is_fg);

    if (pid < 0) {
        printf("Failed to create process\n");
        if (argv_copy != NULL) {
            free_cloned_args(argv_copy, arg_count);
        }
        return -1;
    }

    if (is_background) {
        // Agregar a la lista de jobs y no esperar
        jobs_add(pid, name);
        return 0;
    } else {
        // Foreground: esperar a que termine
        int status = 0;
        int waited = sys_wait_pid(pid, &status);
        if (argv_copy != NULL) {
            free_cloned_args(argv_copy, arg_count);
        }
        if (waited > 0) {
            // Proceso terminado
            return 0;
        }
        return -1;
    }
}

// Ejecuta un comando (builtin o externo)
int shell_execute(char *line) {
    char *argv[MAX_ARGS];
    
    // Tokenizar
    int argc = tokenize(line, argv, MAX_ARGS);
    if (argc == 0) {
        return 0;
    }
    
    // Verificar si hay '&' al final
    int is_background = 0;
    if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
        is_background = 1;
        argv[--argc] = NULL;
    }
    
    if (argc == 0) {
        return 0;
    }
    
    // Buscar en comandos integrados
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(argv[0], builtins[i].name) == 0) {
            if (is_background) {
                printf("Cannot run built-in command in background\n");
                return -1;
            }
            return builtins[i].func(argc, argv);
        }
    }
    
    // Comando externo
    return launch_process(argv[0], argv, is_background);
}

// Loop principal de la shell
void shell_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    char line[MAX_LINE];
    int pos = 0;
    
    // Inicializar el sistema de jobs
    jobs_init();
    
    printf("Welcome to BareBones Shell\n");
    printf("Type 'help' for available commands\n\n");
    
    while (1) {
        // Recolectar zombies de background
        jobs_reap_background();
        
        // Prompt
        printf("$ ");
        
        // Leer línea
        pos = 0;
        while (1) {
            char c;
            int64_t n = sys_read(0, &c);
            
            if (n <= 0) {
                // Error de lectura o EOF
                if (n == -7) {
                    // E_BG_INPUT: no deberíamos llegar aquí en la shell
                    printf("\n[ERROR] Shell cannot read (unexpected BG state)\n");
                }
                continue;
            }
            
            if (c == '\n') {
                line[pos] = '\0';
                printf("\n");
                break;
            } else if (c == '\b' || c == 127) {
                // Backspace
                if (pos > 0) {
                    pos--;
                    printf("\b \b");
                }
            } else if (c >= 32 && c < 127 && pos < MAX_LINE - 1) {
                line[pos++] = c;
                printc(c);
            }
        }
        
        // Ejecutar comando
        if (pos > 0) {
            shell_execute(line);
        }
    }
}
