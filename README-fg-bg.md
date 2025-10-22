# Foreground/Background Implementation

## Diseño General

Esta implementación proporciona soporte completo para control de terminal (TTY discipline) con procesos foreground (FG) y background (BG) en el kernel BareBones.

### Características Principales

- **Un único dueño de TTY**: Solo un proceso puede tener control del terminal (foreground) en un momento dado
- **Input restringido**: Solo el proceso foreground puede leer de stdin
- **Output permitido**: Todos los procesos (FG y BG) pueden escribir a stdout/stderr
- **Shell interactiva**: La shell es el proceso foreground por defecto
- **Operador `&`**: Lanzar procesos en background con el sufijo `&`

## Arquitectura

### Estructuras de Datos

#### TTY (tty.c)
```c
struct tty {
    char buffer[TTY_BUFFER_CAP];
    uint32_t head, tail, size;
    bool eof;
    tty_waiter_t *wait_head, *wait_tail;
    int fg_pid;  // PID del proceso foreground
};
```

#### PCB (sched.h)
```c
typedef struct pcb_t {
    int pid;
    char name[32];
    int priority;
    proc_state_t state;
    bool fg;  // Marca si es foreground
    // ... otros campos ...
};
```

### Funciones Clave

#### Control de TTY

**`void tty_set_foreground(int pid)`**
- Establece el proceso con el PID dado como dueño de la TTY
- Si `pid == -1`, libera el control de la TTY

**`bool tty_can_read(int pid)`**
- Retorna `true` si el proceso con `pid` puede leer de stdin
- Solo el proceso foreground puede leer

**`int tty_get_foreground(void)`**
- Obtiene el PID del proceso foreground actual
- Retorna -1 si no hay ninguno

#### Gestión de Procesos

**`void proc_set_foreground(int pid)`**
- Marca un proceso como foreground en el PCB
- Actualiza la TTY mediante `tty_set_foreground()`
- Desmarca todos los demás procesos

**`int proc_get_foreground_pid(void)`**
- Consulta la TTY para obtener el PID foreground
- Fallback: busca en la tabla de procesos

### Syscalls Modificadas

#### `sys_create_process_ex(entry, argc, argv, name, priority, is_fg)`

Nueva syscall (número 45) que permite crear procesos con control explícito de foreground/background:

```c
// Crear proceso foreground (is_fg = 1)
int64_t pid = sys_create_process_ex(entry, 0, NULL, "test", 2, 1);

// Crear proceso background (is_fg = 0)
int64_t pid = sys_create_process_ex(entry, 0, NULL, "test", 2, 0);
```

**Comportamiento**:
- `is_fg = 1`: Proceso toma control de la TTY inmediatamente
- `is_fg = 0`: Proceso se crea en background, no puede leer stdin

**Nota**: La syscall original `sys_create_process` sigue existiendo para compatibilidad y crea procesos foreground por defecto.

#### `sys_read(int fd, void *buf, int n)`

Modificada para verificar permisos de lectura:

```c
// En fd.c, adaptador para TTY
static int fd_tty_read(int fd, void *buf, int n) {
    pcb_t *cur = sched_current();
    
    // Solo FG puede leer de stdin
    if (fd == FD_STDIN && !tty_can_read(cur->pid)) {
        return E_BG_INPUT;  // -7
    }
    
    return tty_read(...);
}
```

**Comportamiento**:
- Proceso FG: Lee normalmente, se bloquea si no hay datos
- Proceso BG: Retorna inmediatamente con `E_BG_INPUT` (-7)

#### `proc_exit(int code)` y `proc_kill(int pid)`

Modificadas para liberar el control de la TTY:

```c
void proc_exit(int code) {
    // ...
    if (proc->fg) {
        // Devolver TTY al padre (típicamente la shell)
        pcb_t *parent = proc_by_pid(proc->parent_pid);
        if (parent && parent->state != EXITED) {
            proc_set_foreground(parent->pid);
        } else {
            tty_set_foreground(-1);
        }
    }
    // ...
}
```

**Comportamiento**:
- Cuando muere el FG: TTY vuelve a su padre (la shell)
- Si no hay padre válido: TTY queda libre (-1)

## Códigos de Error

Definidos en `Kernel/include/errno.h`:

| Código | Nombre | Descripción |
|--------|--------|-------------|
| 0 | `E_SUCCESS` | Operación exitosa |
| -1 | `E_INVAL` | Argumento inválido |
| -2 | `E_NOMEM` | Sin memoria disponible |
| -3 | `E_NOENT` | Entidad no encontrada |
| -4 | `E_PERM` | Operación no permitida |
| -5 | `E_CHILD` | No hay hijos para esperar |
| -6 | `E_AGAIN` | Recurso temporalmente no disponible |
| **-7** | **`E_BG_INPUT`** | **Proceso BG intentó leer de stdin** |
| -8 | `E_IO` | Error de I/O |
| -9 | `E_BUSY` | Recurso ocupado |
| -10 | `E_DEADLK` | Deadlock detectado |

### Error Crítico: E_BG_INPUT

Este error se retorna cuando un proceso en background intenta leer de stdin. El proceso **NO** se bloquea, sino que recibe el error inmediatamente.

**Ejemplo**:
```c
char buf[64];
int n = sys_read(STDIN, buf);

if (n == E_BG_INPUT) {
    // Proceso en background, no puede leer
    printf("[ERROR] Cannot read from stdin in background\n");
    exit(1);
}
```

## Uso de la Shell

### Comandos Integrados

- **`help`**: Muestra ayuda
- **`jobs`**: Lista procesos en background
- **`fg <pid>`**: Trae un proceso BG a foreground
- **`ps`**: Lista todos los procesos
- **`kill <pid>`**: Mata un proceso
- **`nice <pid> <prio>`**: Cambia prioridad (0-3)
- **`exit`**: Sale de la shell

### Lanzar Procesos

#### Foreground (por defecto)
```bash
$ test_mm
[Proceso ejecuta, shell espera]
[Proceso termina]
$ 
```

#### Background (con `&`)
```bash
$ loop &
[bg] 5 loop
$ 
[loop 5] Starting infinite loop
$ jobs
[0] 5 loop
$ kill 5
Killed process 5
```

### Ejemplos

#### 1. Ejecutar proceso y esperar
```bash
$ test_processes
[Test ejecuta y muestra output]
$ 
```

#### 2. Lanzar en background
```bash
$ loop &
[bg] 6 loop
$ ps
PID   PRIO  STATE  TICKS  FG  NAME
----  ----  -----  -----  --  ----
   1     2  RUN       5   Y  shell
   6     2  READY     5   N  loop
```

#### 3. Múltiples jobs en background
```bash
$ loop &
[bg] 7 loop
$ loop &
[bg] 8 loop
$ jobs
[0] 7 loop
[1] 8 loop
$ kill 7
$ jobs
[1] 8 loop
```

#### 4. Traer a foreground
```bash
$ test_mm &
[bg] 9 test_mm
$ fg 9
[fg] 9
[test_mm ejecuta]
[fg] 9 exited with status 0
```

## Casos de Uso y Comportamiento

### Caso 1: Cat en Background

```bash
$ cat &
[bg] 10 cat
```

**Resultado**: `cat` intenta leer de stdin, recibe `E_BG_INPUT`, imprime error y termina.

### Caso 2: Proceso BG que Solo Escribe

```bash
$ loop &
[bg] 11 loop
[loop 11] Starting infinite loop
[loop 11] Iteration 1
[loop 11] Iteration 2
$ 
```

**Resultado**: El output del loop se mezcla con el prompt, pero la shell sigue respondiendo.

### Caso 3: Kill del Foreground

```bash
$ test_processes
^C
[DEBUG] Killing FG process: 12
$ 
```

**Resultado**: Ctrl+C mata el proceso foreground, la shell recupera el control inmediatamente.

### Caso 4: Proceso BG Muere

Cuando un proceso background termina, la shell lo detecta en el próximo ciclo y lo recolecta:

```bash
$ test_mm &
[bg] 13 test_mm
$ 
[bg] 13 test_mm terminated
```

## Invariantes Verificadas

El sistema garantiza las siguientes invariantes:

1. **Idle existe desde boot**: El proceso idle se crea en `sched_init()` y nunca se destruye
2. **Un único dueño de TTY**: Siempre hay 0 o 1 proceso foreground, nunca más
3. **BG no lee de stdin**: Procesos background reciben `E_BG_INPUT` al intentar leer
4. **Unblock no va a RUNNING**: `proc_unblock()` pone procesos en `READY`, el scheduler decide cuándo ejecutar
5. **Kill libera TTY**: Matar el FG devuelve el control a la shell automáticamente

## Tests Implementados

### Tests Automáticos (test_fg_bg.c)

1. **`test_bg_cannot_read`**: Verifica que BG recibe E_BG_INPUT
2. **`test_shell_regains_tty`**: Shell mantiene control después de lanzar BG
3. **`test_bg_can_write`**: BG puede escribir sin problemas
4. **`test_multiple_bg`**: Múltiples procesos BG simultáneos
5. **`test_fg_normal_read`**: FG puede leer normalmente (manual)

### Ejecutar Tests

```bash
$ test_fg_bg
========================================
   Foreground/Background Tests
========================================

=== Test 1: BG cannot read ===
[test] Created background reader with PID 15
[test_bg] Read correctly returned E_BG_INPUT
[PASS] BG process correctly got E_BG_INPUT

=== Test 2: Shell regains TTY ===
[test] Shell PID: 1
[test] Created background writer with PID 16
[bg_writer] Hello from background!
[bg_writer] Exiting normally
[PASS] Shell still has TTY control

...
```

## Limitaciones Conocidas

1. **No hay grupos de procesos (pgid)**: Implementación simplificada con PID individual
2. **No hay señales SIGTSTP/SIGCONT**: No se puede pausar/reanudar procesos
3. **Output no serializado perfectamente**: Múltiples BG pueden mezclar líneas (mitigado con yields)
4. **Sin control de jobs sofisticado**: No hay estado "stopped" para jobs

## Mejoras Futuras

1. **Process groups (pgid)**: Implementar `setpgid()`, `getpgid()`, `tcsetpgrp()`
2. **Señales**: SIGINT, SIGTSTP, SIGCONT para control completo
3. **Job control**: Estados stopped/continued para `jobs`
4. **Mejor serialización**: Lock por línea en output para evitar mezclas
5. **Redirección**: Soporte para `>`, `<`, `|`

## Referencias de Código

### Archivos Modificados

**Kernel**:
- `Kernel/include/tty.h` - Nuevas funciones de control TTY
- `Kernel/tty.c` - Implementación de fg_pid y verificación
- `Kernel/include/errno.h` - Códigos de error (nuevo)
- `Kernel/fd.c` - Verificación de FG en read
- `Kernel/proc.c` - Gestión de foreground en exit/kill
- `Kernel/include/sched.h` - Nueva función `proc_set_foreground()`

**Userland**:
- `Userland/SampleCodeModule/shell.c` - Shell con soporte `&` (nuevo)
- `Userland/SampleCodeModule/sh/jobs.c` - Gestión de jobs (existente, mejorado)
- `Userland/SampleCodeModule/tests/test_fg_bg.c` - Tests automáticos (nuevo)
- `Userland/SampleCodeModule/loop.c` - Programa de prueba (nuevo)

## Compilación y Ejecución

```bash
# Compilar todo
make clean
make all

# Ejecutar
./run.sh

# En la shell del kernel:
$ help
$ test_fg_bg
$ loop &
$ jobs
$ kill <pid>
```

## Notas de Implementación

- **Thread-safety**: Funciones TTY usan `irq_save/restore_local()` para proteger secciones críticas
- **No busy-wait**: Los procesos bloqueados en lectura se ponen en `wait_head` de la TTY
- **Limpieza garantizada**: Zombies se recolectan en `collect_zombies()`, huérfanos se reasignan
- **Idle failsafe**: Si no hay procesos READY, el scheduler ejecuta idle automáticamente

---

**Autor**: TP2-Kernel Team  
**Fecha**: Octubre 2025  
**Versión**: 1.0
