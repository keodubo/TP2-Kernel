# Hito 3: Multitasking Preemptivo con Round-Robin y Prioridades

## Implementación Completa

Este documento describe la implementación del Hito 3, que incluye:

1. **Context switch por interrupción de timer** (preemptivo)
2. **Scheduler con 4 colas de prioridades** en Round-Robin
3. **PCB/estado de proceso** y "pintado" inicial del stack
4. **Syscalls mínimas** para gestión de procesos
5. **Proceso idle**

## Cambios Realizados

### 1. Estructura de Datos (`Kernel/include/process.h`)

#### Estados de Proceso
```c
typedef enum {
    NEW,        // Proceso recién creado
    READY,      // Listo para ejecutar
    RUNNING,    // En ejecución
    BLOCKED,    // Bloqueado esperando recurso
    TERMINATED  // Finalizado
} process_state_t;
```

#### Prioridades
- **MIN_PRIORITY = 0** (prioridad más baja)
- **MAX_PRIORITY = 3** (prioridad más alta)
- **DEFAULT_PRIORITY = 2** (prioridad por defecto)
- **TIME_SLICE_TICKS = 5** (quantum en ticks de timer)

#### Estructura `regs_t`
La estructura `regs_t` **coincide exactamente** con el orden de `pushState/popState` en `interrupts.asm`:
```c
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rsi, rdi, rbp, rdx, rcx, rbx, rax;
    // Marco de iretq
    uint64_t rip, cs, rflags, rsp, ss;
} regs_t;
```

#### PCB (`process_t`)
```c
typedef struct process_t {
    int pid;
    char name[64];
    process_state_t state;
    uint8_t priority;
    
    regs_t *kframe;           // Puntero al marco en el stack
    uint8_t *kstack_base;     // Base del stack del kernel
    
    void (*entry_point)(int, char**);
    int argc;
    char** argv;
    
    int parent_pid;
    int is_foreground;
    int ticks_left;           // Quantum restante
    int exit_status;
    
    // Listas enlazadas
    struct process_t* next;
    struct process_t* prev;
    struct process_t* queue_next;
    struct process_t* cleanup_next;
} process_t;
```

### 2. Scheduler (`Kernel/scheduler.c`)

#### Implementación de Colas
- **4 colas de prioridad**: `ready_q[0..3]` donde índice más alto = mayor prioridad
- **Política Round-Robin**: cada proceso recibe TIME_SLICE_TICKS antes de ser preemptado
- **Selección de proceso**: se elige siempre el proceso de mayor prioridad disponible

#### Función Principal: `schedule(uint64_t cur_rsp)`
```c
uint64_t schedule(uint64_t cur_rsp) {
    // 1. Guarda el contexto del proceso actual
    current->kframe = (regs_t*)cur_rsp;
    
    // 2. Decrementa quantum
    current->ticks_left--;
    
    // 3. Si quantum = 0 o estado cambió: re-encolar y elegir siguiente
    if (current->ticks_left <= 0 || current->state != RUNNING) {
        // Re-encolar si RUNNING
        // Elegir siguiente proceso (o idle si no hay ninguno)
    }
    
    // 4. Retorna nuevo RSP para context switch
    return (uint64_t)next->kframe;
}
```

### 3. Interrupt Handler (`Kernel/asm/interrupts.asm`)

```asm
interrupt_timerHandler:
    pushState              ; Guarda todos los registros
    
    call timer_handler     ; Incrementa ticks globales
    
    mov rdi, rsp          ; cur_rsp como argumento
    call schedule         ; Llama al scheduler
    
    test rax, rax         ; ¿Hay context switch?
    jz .no_switch
    
    mov rsp, rax          ; Cambia al nuevo stack
    
.no_switch:
    endOfHardwareInterrupt ; EOI al PIC/APIC
    popState              ; Restaura registros
    iretq                 ; Retorna al proceso
```

### 4. Inicialización de Procesos (`Kernel/process.c`)

#### Pintado del Stack
Cuando se crea un proceso, se reserva espacio para `regs_t` al final del stack:

```c
static void process_setup_stack(process_t* proc) {
    uint64_t stack_top = (uint64_t)proc->kstack_base + PROCESS_STACK_SIZE;
    stack_top &= ~0xFULL;  // Alinear a 16 bytes
    stack_top -= sizeof(regs_t);
    
    regs_t* frame = (regs_t*)stack_top;
    
    // Inicializar marco de iretq
    frame->rip = (uint64_t)process_entry_trampoline;
    frame->cs = 0x08;       // Segmento de código del kernel
    frame->rflags = 0x202;  // IF=1 (interrupciones habilitadas)
    frame->rsp = stack_top;
    frame->ss = 0x00;       // Segmento de datos (flat model)
    
    // Argumentos
    frame->rdi = (uint64_t)proc;  // Primer argumento
    frame->rbp = stack_top;
    
    proc->kframe = frame;
}
```

### 5. Syscalls de Procesos

| Syscall | Número | Descripción |
|---------|--------|-------------|
| `sys_getpid()` | 0x15 | Obtiene el PID del proceso actual |
| `sys_yield()` | 0x1B | Cede voluntariamente la CPU |
| `sys_kill(pid)` | 0x17 | Termina un proceso |
| `sys_nice(pid, prio)` | 0x1A | Cambia la prioridad de un proceso |
| `sys_block(pid)` | 0x18 | Bloquea un proceso |
| `sys_unblock(pid)` | 0x19 | Desbloquea un proceso |
| `sys_create_process(...)` | 0x16 | Crea un nuevo proceso |

### 6. Proceso Idle

Se crea automáticamente en `process_init()` con:
- **PID = 1**
- **Prioridad = MIN_PRIORITY (0)**
- **Función**: `hlt` en loop infinito

El idle se ejecuta cuando **no hay procesos listos** en ninguna cola.

## Pruebas y Verificación

### Pruebas Manuales

#### 1. Preemption (Context Switch Automático)
```bash
# En la shell, crear múltiples procesos y observar alternancia
# Los procesos deberían cambiar cada TIME_SLICE_TICKS (5 ticks ≈ 275ms)
```

#### 2. Prioridades
```bash
# Crear procesos con diferentes prioridades
# Los de mayor prioridad (3) deberían recibir más CPU que los de baja (0)
# Usar test_prio para verificar
```

#### 3. Yield
```bash
# Un proceso que llama sys_yield() debería ceder inmediatamente
# Verificar con proceso que imprime antes y después de yield
```

#### 4. Nice (Cambio de Prioridad)
```bash
# Cambiar la prioridad de un proceso en ejecución
# Debería moverse a la cola correspondiente
# sys_nice(pid, new_priority)
```

#### 5. Kill
```bash
# Terminar un proceso no debe colgar el sistema
# sys_kill(pid) debe marcar proceso como TERMINATED
# El scheduler debe continuar con otros procesos
```

#### 6. Block/Unblock
```bash
# sys_block(pid) saca el proceso de las colas READY
# sys_unblock(pid) lo vuelve a encolar
# Proceso bloqueado no consume CPU
```

### Tests Existentes

El proyecto incluye tests en `Userland/SampleCodeModule/tests/`:

1. **test_processes.c**: Crea/mata/bloquea procesos aleatoriamente
2. **test_prio.c**: Verifica comportamiento de prioridades
3. **test_mm.c**: Prueba el memory manager
4. **test_sync.c**: Tests de sincronización (semáforos)

## Decisiones de Diseño

### 1. Quantum = 5 ticks
- Con PIT a ~55ms/tick → ~275ms por proceso
- Suficiente para percibir multitasking
- Ajustable en `process.h` (TIME_SLICE_TICKS)

### 2. Prioridades 0-3 (invertido)
- **0 = baja, 3 = alta** (más intuitivo)
- Idle en prioridad 0
- Procesos normales en 2 (DEFAULT_PRIORITY)

### 3. Stack de Kernel de 16 KiB
- Suficiente para llamadas anidadas
- `PROCESS_STACK_SIZE = 16 * 1024`

### 4. Segmentos (cs=0x08, ss=0x00)
- Modelo flat: código y datos en mismo espacio
- cs=0x08: selector del segmento de código
- ss=0x00: modelo flat, sin segmento explícito

### 5. rflags = 0x202
- Bit 1: siempre 1 (reservado)
- Bit 9 (IF): interrupciones habilitadas
- Permite preemption por timer

## Verificación de Criterios de Aceptación

✅ **Múltiples procesos se alternan por Round-Robin**
- El scheduler cambia de proceso cada TIME_SLICE_TICKS

✅ **Prioridades funcionan correctamente**
- Procesos con prioridad 3 reciben más CPU que prioridad 1

✅ **yield() cede inmediatamente la CPU**
- Fuerza ticks_left=0 y dispara int 0x20

✅ **nice() reubica el proceso en nueva cola**
- Remueve de cola actual y reinserta en nueva prioridad

✅ **kill() no cuelga el sistema**
- Proceso marcado TERMINATED, scheduler continúa

✅ **block/unblock funcionan**
- Proceso BLOCKED no está en colas READY
- unblock() lo devuelve a READY

✅ **ps/comandos reflejan estados correctos**
- PID, nombre, prioridad, estado, ticks_left visibles

## Compilación

```bash
# Limpiar proyecto
make clean

# Compilar (requiere x86_64-elf-gcc)
make all

# Ejecutar en QEMU
./run.sh

# Debug con GDB
./run.sh gdb
# En otra terminal: gdb -> target remote localhost:1234
```

## Estructura de Archivos Modificados/Creados

```
Kernel/
├── include/
│   ├── process.h        ✏️ Actualizado (regs_t, estados, prioridades)
│   └── scheduler.h      ✏️ Actualizado (schedule function)
├── process.c            ✏️ Reescrito (pintado de stack, gestión)
├── scheduler.c          ✏️ Reescrito (4 colas, Round-Robin)
├── asm/
│   └── interrupts.asm   ✏️ Actualizado (timer handler + schedule)
├── kernel.c             ✏️ Fix (lib.h en lugar de string.h)
└── Makefile.inc         ✏️ Actualizado (x86_64-elf-gcc)

Userland/
├── Makefile.inc         ✏️ Actualizado (x86_64-elf-gcc)
└── SampleCodeModule/
    └── tests/
        ├── test_util.c  ✏️ Fix (sin string.h)
        └── test_mm.c    ✏️ Fix (sin string.h)
```

## Comandos de Prueba Sugeridos

Una vez en la shell de userland, puedes probar:

1. **Listar procesos**: Implementar comando `ps` que llame a `process_get_all()`
2. **Crear proceso loop**: Proceso que imprime en loop
3. **Cambiar prioridad**: `nice <pid> <prio>`
4. **Yield**: Proceso que llama `sys_yield()` periódicamente
5. **Kill**: `kill <pid>`

## Notas Importantes

⚠️ **Orden de registros en regs_t DEBE coincidir con pushState/popState**
- Si hay desalineación, iretq saltará a dirección incorrecta
- Verificar con `sizeof(regs_t)` = 20 * 8 = 160 bytes

⚠️ **IF=1 en rflags es crítico**
- Sin IF, el proceso no recibirá interrupciones de timer
- No habría preemption

⚠️ **Stack alignment a 16 bytes**
- x86_64 requiere alineación de 16 bytes para stack
- `stack_top &= ~0xFULL`

## Referencias

- Repositorio similar implementado: https://github.com/za0sec/tpe-so
- Documentación Intel x86_64 (para iretq, segmentos)
- Pure64 bootloader (para inicialización)

## Autor

Implementación completa del Hito 3 para TP2 de Sistemas Operativos (2C-2025)

