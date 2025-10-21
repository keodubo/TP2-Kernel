#ifndef JOBS_H
#define JOBS_H

#include <stdint.h>

#define MAX_JOBS 32

typedef struct {
    int pid;
    char name[32];
    int active;
} job_t;

// Inicializar el sistema de jobs
void jobs_init(void);

// Agregar un proceso en background
void jobs_add(int pid, const char *name);

// Remover un proceso de la lista
void jobs_remove(int pid);

// Limpiar procesos zombies en background (no bloqueante)
void jobs_reap_background(void);

// Listar todos los jobs activos
void jobs_list(void);

// Verificar si un PID está en la lista de jobs
int jobs_has(int pid);

#endif
