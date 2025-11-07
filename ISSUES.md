# Problemas y Limitaciones Conocidas

## Race Condition Potencial en Snapshots de Procesos

### Descripción

Existe una condición de carrera potencial en la función `proc_snapshot()` ubicada en [Kernel/process/proc.c](Kernel/process/proc.c), que se utiliza para obtener información sobre todos los procesos del sistema.

### Ubicación

**Archivo**: `Kernel/process/proc.c`
**Función**: `int proc_snapshot(proc_info_t* infos, int max_count)`

### Problema

La función itera sobre la tabla de procesos para recopilar información sin utilizar mecanismos de sincronización (como spinlocks) para proteger el acceso concurrente. Esto podría causar inconsistencias si:

1. Un proceso es creado o destruido mientras se realiza el snapshot
2. El estado de un proceso cambia durante la iteración
3. Múltiples CPUs acceden simultáneamente a la tabla de procesos

### Severidad

**Baja a Media**

- **Impacto**: La información del snapshot podría ser temporalmente inconsistente
- **Probabilidad**: Baja en un sistema de un solo núcleo con interrupciones deshabilitadas durante operaciones críticas
- **Consecuencias**: Información incorrecta mostrada al usuario (comando `ps`), pero sin corrupción de datos del kernel

### Casos de Uso Afectados

- Comando `ps` en la shell
- Syscall `sys_proc_snapshot()` cuando se invoca concurrentemente con creación/destrucción de procesos

### Mitigación Actual

El sistema actualmente mitiga parcialmente este problema mediante:

1. Operaciones atómicas en secciones críticas de creación/destrucción de procesos
2. Ejecución en un entorno de un solo núcleo (no hay verdadero paralelismo)
3. Uso de spinlocks en otras operaciones críticas del scheduler

### Solución Propuesta

Para resolver completamente este problema se recomienda:

```c
int proc_snapshot(proc_info_t* infos, int max_count) {
    if (infos == NULL || max_count <= 0) {
        return 0;
    }

    // Agregar protección con spinlock
    uint64_t flags = irq_save();

    int count = 0;
    for (int i = 0; i < MAX_PROCS && count < max_count; i++) {
        if (process_table[i].state != UNUSED && process_table[i].state != ZOMBIE) {
            // Copiar información del proceso
            copy_proc_info(&infos[count], &process_table[i]);
            count++;
        }
    }

    irq_restore(flags);
    return count;
}
```

### Referencias

- Discusión similar en semáforos: `Kernel/ipc/semaphore.c` usa spinlocks correctamente
- Pipes también usan secciones críticas: `Kernel/ipc/pipe.c`

### Notas

Este problema fue identificado durante la revisión de calidad del código y documentado para referencia futura. No representa un riesgo crítico para la funcionalidad actual del sistema.

---

**Última actualización**: 2025-11-06
**Estado**: Documentado, no crítico
