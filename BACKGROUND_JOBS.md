# Background Jobs Implementation (&)

## Implementación

Se agregó soporte para ejecutar comandos en background usando el operador `&` al final del comando.

### Archivos Modificados

1. **Userland/SampleCodeModule/sh/jobs.h** - Header con estructura de jobs
2. **Userland/SampleCodeModule/sh/jobs.c** - Implementación de la tabla de jobs
3. **Userland/SampleCodeModule/kitty.c** - Modificaciones en el shell:
   - Parsing de `&` en `newLine()`
   - Llamada a `jobs_reap_background()` en cada iteración del loop principal
   - Modificación de `cmd_loop()` para soportar ejecución en background
   - Nuevo comando `jobs` para listar procesos en background
4. **Userland/SampleCodeModule/Makefile** - Agregado compilación de sh/jobs.c

### Arquitectura

#### Estructura de Jobs
```c
typedef struct {
    int pid;
    char name[32];
    int active;
} job_t;
```

- Máximo 32 jobs simultáneos (configurable con `MAX_JOBS`)
- Cada job almacena: PID, nombre del proceso, estado activo

#### Supresión de Output en Background

Los procesos en background ejecutan con el argumento `"silent"` para suprimir su output:
- **Foreground:** `loop_process(0, NULL)` → imprime `[loop X] .`
- **Background:** `loop_process(1, ["silent"])` → NO imprime

Esto evita que el output del background interfiera con el prompt de la shell.

#### Flujo de Ejecución

**Foreground (sin `&`):**
1. Shell parsea comando → detecta NO hay `&`
2. `is_background = 0`
3. `cmd_loop()` llama `sys_create_process()` → obtiene PID
4. Shell llama `sys_wait_pid(pid)` → **BLOQUEA** hasta que termina
5. Proceso marcado como `fg = true` en kernel
6. Ctrl+C mata el proceso foreground
7. Shell recupera control, limpia buffer, muestra prompt

**Background (con `&`):**
1. Shell parsea comando → detecta `&` al final
2. `is_background = 1`, remueve `&` de la línea
3. `cmd_loop()` crea argv con `["silent"]`
4. `cmd_loop()` llama `sys_create_process(loop_process, 1, ["silent"], ...)` → obtiene PID
5. `loop_process` detecta argumento "silent" → **NO imprime output**
6. Shell llama `jobs_add(pid, name)` → registra en tabla
7. Shell imprime `[bg] <pid> <name>` + prompt
8. Shell **NO** llama `sys_wait_pid()` → **RETORNA INMEDIATAMENTE**
9. Proceso corre en background silencioso (no imprime, no recibe teclado)
10. Shell vuelve al prompt limpio

#### Reaping de Zombies

En cada iteración del loop principal (`kitty()`):
```c
while (1 && !terminate) {
    jobs_reap_background();  // ← Limpia zombies
    drawCursor();
    c = getChar();
    printLine(c);
}
```

`jobs_reap_background()`:
1. Obtiene snapshot de todos los procesos con `sys_proc_snapshot()`
2. Para cada job activo, verifica si el PID existe
3. Si existe y estado == EXITED (4):
   - Llama `sys_wait_pid()` para recolectar zombie
   - Imprime `[bg] <pid> <name> terminated`
   - Marca job como inactivo
4. Si no existe en snapshot:
   - Otro proceso ya lo recolectó → marca job como inactivo

### Funciones Principales

```c
void jobs_init(void);                   // Inicializa tabla de jobs
void jobs_add(int pid, const char *name); // Agrega job en background
void jobs_remove(int pid);               // Remueve job manualmente
void jobs_reap_background(void);         // Recolecta zombies (no bloqueante)
void jobs_list(void);                    // Lista jobs activos
int jobs_has(int pid);                   // Verifica si PID está en jobs
```

### Nuevo Comando: `jobs`

Lista todos los procesos en background activos:
```
$ jobs
[0] 3 loop-0
[1] 5 loop-1
```

Si no hay jobs:
```
$ jobs
No background jobs
```

## Cómo Probar

### Compilación

1. Asegúrate de tener el Toolchain compilado:
```bash
cd Toolchain
make all
```

2. Compila el kernel:
```bash
cd ..
make clean && make
```

3. Ejecuta el OS:
```bash
./run.sh
```

### Pruebas Básicas

#### 1. Loop en Foreground (comportamiento normal)
```
$ loop -p 2
```
**Resultado esperado:**
- El loop imprime su salida
- Shell **NO** vuelve al prompt hasta terminar el proceso
- Ctrl+C mata el proceso y retorna al prompt

#### 2. Loop en Background
```
$ loop -p 2 &
```
**Resultado esperado:**
- Imprime: `[bg] 3 loop-0` (PID puede variar)
- Imprime prompt: `user $>`
- Shell **vuelve INMEDIATAMENTE** al prompt
- El loop **NO imprime** su output (corre silencioso)
- Puedes escribir más comandos sin interferencia

#### 3. Múltiples Jobs en Background
```
$ loop -p 2 &
[bg] 3 loop-0
$ loop -p 1 &
[bg] 4 loop-1
$ loop &
[bg] 5 loop-2
$ jobs
[0] 3 loop-0
[1] 4 loop-1
[2] 5 loop-2
```

#### 4. Verificar con `ps`
```
$ loop -p 2 &
[bg] 3 loop-0
$ ps
```
**Resultado esperado:**
- Verás el proceso `loop-0` con PID 3 en la lista
- Estado: RUNNING o READY
- FG: `BG` (background)
- La shell (PID 2) tiene FG: `FG` (foreground)

#### 5. Terminar Job en Background
```
$ loop &
[bg] 3 loop-0
$ kill 3
$ jobs
No background jobs
```

#### 6. Reaping Automático
```
$ loop &
[bg] 3 loop-0
```
**Espera unos segundos** (el loop eventualmente termina)
```
[bg] 3 loop-0 terminated
$ jobs
No background jobs
```

### Casos de Prueba Avanzados

#### Test 1: Background + Foreground Simultáneos
```
$ loop -p 2 &
[bg] 3 loop-0
$ loop -p 1
```
- Job background (PID 3) corre en background
- Job foreground (PID 4) toma control del shell
- Ctrl+C mata solo el foreground (PID 4)
- Background sigue corriendo

#### Test 2: Muchos Jobs
```
$ loop & loop & loop & loop & loop &
$ jobs
```
Verifica que todos están listados

#### Test 3: Interacción con Tests
```
$ test_processes 5 &
[bg] 3 test_processes
$ ps
```
Verás 5+ procesos worker corriendo en background

### Comportamiento Esperado

✅ **Correcto:**
- `loop &` retorna prompt inmediatamente
- `jobs` lista procesos activos
- `ps` muestra jobs con `FG: BG`
- Cuando termina, imprime `[bg] PID nombre terminated`
- Ctrl+C **NO** mata jobs background
- Shell sigue responsivo con jobs corriendo

❌ **Incorrecto (bug):**
- `loop &` bloquea el shell (si esto pasa, `is_background` no se detectó)
- Jobs no aparecen en `jobs` (problema en `jobs_add()`)
- Jobs quedan zombies (problema en `jobs_reap_background()`)
- Ctrl+C mata jobs background (problema en kernel, solo debe matar foreground)

## Consideraciones del Foro

Según la especificación del foro:

1. ✅ **Shell mantiene foreground:** La shell es foreground cuando no hay proceso hijo en espera
2. ✅ **Background no lee teclado:** Procesos background no reciben input del usuario
3. ✅ **Background puede imprimir:** Los procesos pueden escribir a stdout (útil para ver errores de tests)
4. ✅ **Reaping no bloqueante:** `jobs_reap_background()` usa `sys_proc_snapshot()` para verificar estados sin bloquear

## Debugging

Si algo no funciona:

1. **Job no retorna prompt:**
   - Verifica que `&` se detectó correctamente
   - Debug: Agrega `printf("is_background=%d\n", is_background);` en `newLine()`

2. **Job no aparece en `jobs`:**
   - Verifica que `jobs_add()` se llama
   - Debug: Agrega prints en `jobs_add()`

3. **Zombie no se recolecta:**
   - Verifica que `sys_proc_snapshot()` funciona
   - Debug: Imprime `proc_count` en `jobs_reap_background()`

4. **Ctrl+C mata background:**
   - Problema en el kernel, no en userland
   - El kernel debe solo matar procesos con `fg=true`

## Notas de Implementación

- **Variable global `is_background`:** Simple y efectivo para pasar la flag entre `newLine()` y los comandos
- **Polling en loop:** Overhead mínimo, `sys_proc_snapshot()` es rápido
- **MAX_JOBS=32:** Suficiente para casos de uso típicos, ajustable si necesitas más
- **Estado EXITED=4:** Hardcoded, podría mejorarse con enum compartido
