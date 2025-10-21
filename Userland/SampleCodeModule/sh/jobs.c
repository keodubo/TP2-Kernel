#include "jobs.h"
#include <stdio.h>
#include <sys_calls.h>

static job_t jobs[MAX_JOBS];
static int jobs_initialized = 0;

void jobs_init(void) {
    if (jobs_initialized) {
        return;
    }
    
    for (int i = 0; i < MAX_JOBS; i++) {
        jobs[i].pid = -1;
        jobs[i].active = 0;
        jobs[i].name[0] = '\0';
    }
    jobs_initialized = 1;
}

void jobs_add(int pid, const char *name) {
    jobs_init();
    
    // Buscar slot libre
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) {
            jobs[i].pid = pid;
            jobs[i].active = 1;
            
            // Copiar nombre
            int j = 0;
            while (j < 31 && name && name[j] != '\0') {
                jobs[i].name[j] = name[j];
                j++;
            }
            jobs[i].name[j] = '\0';
            
            printf("[bg] %d %s\n", pid, name);
            return;
        }
    }
    
    printf("[jobs] Warning: job table full\n");
}

void jobs_remove(int pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active && jobs[i].pid == pid) {
            jobs[i].active = 0;
            jobs[i].pid = -1;
            return;
        }
    }
}

void jobs_reap_background(void) {
    jobs_init();
    
    // Buffer para obtener snapshot de procesos
    proc_info_t procs[64];
    int64_t proc_count = sys_proc_snapshot(procs, 64);
    
    // Verificar cada job activo
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) {
            continue;
        }
        
        // Verificar si el PID todavía existe en la lista de procesos
        int found = 0;
        for (int j = 0; j < proc_count; j++) {
            if (procs[j].pid == jobs[i].pid) {
                // Estado 4 = EXITED (definido en el kernel)
                if (procs[j].state == 4) {
                    // Proceso en estado EXITED, recolectar
                    int status = 0;
                    sys_wait_pid(jobs[i].pid, &status);
                    printf("[bg] %d %s terminated\n", jobs[i].pid, jobs[i].name);
                    jobs[i].active = 0;
                    jobs[i].pid = -1;
                } else {
                    // Proceso aún corriendo
                    found = 1;
                }
                break;
            }
        }
        
        // Si no se encontró, el proceso ya no existe (fue recolectado por otro wait)
        if (!found) {
            jobs[i].active = 0;
            jobs[i].pid = -1;
        }
    }
}

void jobs_list(void) {
    jobs_init();
    
    int found = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active) {
            printf("[%d] %d %s\n", i, jobs[i].pid, jobs[i].name);
            found = 1;
        }
    }
    
    if (!found) {
        printf("No background jobs\n");
    }
}

int jobs_has(int pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active && jobs[i].pid == pid) {
            return 1;
        }
    }
    return 0;
}
