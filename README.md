# TP2 - Kernel Operating System

Sistema operativo básico desarrollado sobre x64BareBones con gestión de memoria, procesos, sincronización e IPC.

## 📦 Entorno y Compilación

### Requisitos Previos

- Docker (para ejecución en contenedor multiplataforma)
- O las siguientes herramientas instaladas:
  - `nasm`
  - `qemu`
  - `gcc`
  - `make`

### Compilación

El proyecto se compila con `-Wall -Wextra -Werror` para garantizar calidad de código.

#### Compilación estándar (First Fit)

```bash
make clean all
```

#### Compilación con Buddy System

```bash
make buddy
```

o alternativamente:

```bash
make clean
make MM_FLAG=-DUSE_BUDDY_SYSTEM all
```

### Ejecución

#### Con Docker (recomendado)

```bash
make docker
```

Dentro del contenedor:

```bash
make clean all
./run.sh
```

#### Sin Docker

```bash
./run.sh
```

O manualmente:

```bash
qemu-system-x86_64 -cdrom Image/x64BareBonesImage.iso -m 512 -serial stdio
```

### Limpieza

```bash
make clean
```

## 🎯 Características Implementadas

### 1. Gestión de Memoria Física

#### Memory Managers

El sistema soporta dos algoritmos de gestión de memoria, seleccionables en tiempo de compilación:

- **First Fit** (por defecto): Asigna el primer bloque libre que sea suficientemente grande.
- **Buddy System**: Divide la memoria en bloques de tamaño potencia de 2, mejorando fragmentación interna.

**Interfaz común:**

Ambos managers exponen la misma API:
- `mm_malloc(size_t size)`: Reservar memoria
- `mm_free(void* ptr)`: Liberar memoria
- `mm_get_info()`: Obtener estadísticas de memoria

#### System Calls

- `sys_malloc(size_t size)` / `sys_alloc(size_t size)`: Reservar memoria en user space.
- `sys_free(void* ptr)`: Liberar memoria previamente asignada.
- `sys_mem_info(memory_info_t* info)`: Consultar información de memoria (total, libre, usada).
- `sys_mm_get_stats(mm_stats_t* stats)`: Obtener estadísticas detalladas del heap.

#### Tests

- `test_mm [bytes]`: Ejecuta un test exhaustivo del gestor de memoria:
  - Asigna múltiples bloques hasta agotar memoria disponible
  - Escribe patrones en cada bloque
  - Verifica integridad de los datos
  - Libera todos los bloques
  - Repite el ciclo infinitamente
  - Por defecto usa 100,000,000 bytes si no se especifica tamaño

**Ejemplo de uso:**
```bash
test_mm 50000000    # Test con 50MB
test_mm            # Test con 100MB (default)
```

**Verificación:** El test pasa sin solapamientos de memoria cuando se ejecuta correctamente.

### 2. Procesos, Context Switching y Scheduling

#### Características

- **Multitasking preemptivo**: El scheduler roba CPU a procesos que agotan su quantum.
- **Round Robin con prioridades**: Procesos con mayor prioridad (números menores) ejecutan más frecuentemente.
- **Context switching completo**: Se guardan/restauran todos los registros, stack pointer, y base pointer.
- **Foreground y Background**: Soporte para procesos que controlan la TTY y procesos en background.

#### System Calls

- `sys_create_process(entry_point, argc, argv, name, priority)`: Crear proceso con parámetros.
- `sys_create_process_ex(entry_point, argc, argv, name, priority, is_fg)`: Crear proceso especificando foreground/background.
- `sys_exit(int status)`: Terminar proceso actual.
- `sys_kill(int pid)`: Terminar proceso por PID.
- `sys_getpid()`: Obtener PID del proceso actual.
- `sys_proc_snapshot(proc_info_t* info, int max)`: Listar todos los procesos con nombre, id, prioridad, stack, bp, fg/bg.
- `sys_nice(int pid, uint8_t priority)`: Modificar prioridad de un proceso.
- `sys_block(int pid)`: Bloquear proceso.
- `sys_unblock(int pid)`: Desbloquear proceso.
- `sys_yield()`: Ceder CPU inmediatamente.
- `sys_wait_pid(int pid, int* status)`: Esperar a que termine un proceso hijo (con `pid=-1` espera cualquier hijo).

#### Comandos de la Shell

- `ps`: Lista todos los procesos con información detallada:
  ```
  PID   PRIO STATE  TICKS FG        SP                BP                NAME
  1     0    RUN    5     Y         0x000000001234     0x000000005678    shell
  ```

- `loop [-p priority]`: Crea un proceso que hace loop infinito para testing.
  ```bash
  loop -p 3    # Loop con prioridad 3
  loop         # Loop con prioridad por defecto
  ```

- `kill <pid>`: Mata un proceso por su PID.
  ```bash
  kill 5
  ```

- `nice <pid> <priority>`: Cambia la prioridad de un proceso.
  ```bash
  nice 5 1    # Cambiar PID 5 a prioridad 1
  ```

- `block <pid>`: Bloquea un proceso.
- `unblock <pid>`: Desbloquea un proceso.
- `yield`: Hace que el proceso actual ceda la CPU inmediatamente.

- `waitpid [pid|-1]`: Espera a que termine un proceso hijo:
  ```bash
  waitpid 5    # Espera a PID 5
  waitpid -1   # Espera cualquier hijo
  waitpid      # Espera cualquier hijo (default)
  ```

#### Tests

- `test_processes [n]`: Test de gestión de procesos:
  - Crea `n` procesos workers (default: 10)
  - Los bloquea y desbloquea aleatoriamente
  - Los mata al finalizar
  - Verifica que el scheduler funcione correctamente

- `test_priority [n]`: Demostración de scheduling con prioridades:
  - Crea procesos con diferentes prioridades
  - Muestra cómo los procesos de mayor prioridad ejecutan más frecuentemente
  - Por defecto crea 5 procesos

**Características especiales:**

- ✅ El sistema retorna control a la shell tras ejecutar los tests.
- ✅ Soporta procesos foreground y background.
- ✅ Timer tick funcionando para preemptión.
- ✅ Los tests se ejecutan como procesos separados.

### 3. Sincronización

#### Semáforos

Implementación completa de semáforos **sin busy waiting**:
- Los procesos se bloquean cuando esperan en un semáforo con valor 0.
- Se despiertan en orden FIFO cuando otro proceso hace `up`.
- Compartibles entre procesos no relacionados mediante nombres.

#### System Calls

- `sys_sem_open(const char* name, unsigned init)`: Abre/crea semáforo por nombre.
  - Retorna un handle (entero) que identifica el semáforo.
  - Si el semáforo no existe, lo crea con el valor inicial especificado.
  - Si ya existe, retorna el handle existente.

- `sys_sem_wait(int handle)`: Operación `down` (decrementar o bloquearse).
  - Decrementa el contador si es > 0.
  - Si el contador es 0, bloquea al proceso hasta que otro haga `post`.

- `sys_sem_post(int handle)`: Operación `up` (incrementar o despertar).
  - Incrementa el contador si no hay procesos esperando.
  - Despierta exactamente un proceso en orden FIFO si hay esperando.

- `sys_sem_close(int handle)`: Cierra referencia al semáforo.
- `sys_sem_unlink(const char* name)`: Elimina el semáforo del namespace cuando todas las referencias están cerradas.

#### Ejemplo de Uso

```c
int handle = sys_sem_open("mutex", 1);  // Mutex inicializado en 1
sys_sem_wait(handle);
/* sección crítica */
sys_sem_post(handle);
sys_sem_close(handle);
sys_sem_unlink("mutex");
```

#### Tests

- `test_no_synchro [n]`: Ejecuta `2*n` workers que incrementan/decrementan un contador compartido **sin semáforos**.
  - El resultado final es **variable** en cada ejecución.
  - Demuestra la condición de carrera.

- `test_synchro [n] [use_sem]`: Ejecuta la versión sincronizada:
  - Con `use_sem=1` (default): resultado final es **determinísticamente 0**.
  - Con `use_sem=0`: colapsa al comportamiento no sincronizado.
  - Demuestra que los semáforos funcionan correctamente.

**Ejemplos:**
```bash
test_no_synchro 5      # Race condition, resultado variable
test_synchro 5         # Sincronizado, resultado siempre 0
test_synchro 5 0       # Sin semáforos (igual que test_no_synchro)
```

### 4. Inter-Process Communication (IPC)

#### Pipes Unidireccionales

- **Operaciones bloqueantes**: Lectura y escritura bloquean cuando el pipe está vacío/lleno respectivamente.
- **Compartibles entre procesos**: Los pipes pueden ser compartidos mediante identificadores (nombres).
- **Integración con TTY**: Un proceso puede leer/escribir indistintamente desde pipe o terminal según el file descriptor.

#### System Calls

- `sys_pipe_open(const char* name, int mode)`: Abre/crea un pipe por nombre.
  - `mode=0`: Modo lectura.
  - `mode=1`: Modo escritura.
  - Retorna un file descriptor (entero).

- `sys_pipe_read(int fd, void* buf, int count)`: Lee desde el pipe.
  - Bloquea si el pipe está vacío.
  - Retorna número de bytes leídos.

- `sys_pipe_write(int fd, const void* buf, int count)`: Escribe en el pipe.
  - Bloquea si el pipe está lleno.
  - Retorna número de bytes escritos.

- `sys_pipe_close(int fd)`: Cierra el file descriptor del pipe.
- `sys_pipe_unlink(const char* name)`: Elimina el pipe cuando todas las referencias están cerradas.

#### Comando mvar

Implementa un "Mutable Variable" (MVar) usando semáforos y pipes:

```bash
mvar <writers> <readers>
```

**Descripción:**
- Crea `writers` procesos escritores y `readers` procesos lectores.
- Los escritores escriben letras ('A', 'B', ...) al MVar.
- Los lectores consumen las letras y las imprimen en color.
- Coordinación mediante semáforos: solo un escritor puede escribir a la vez, solo un lector puede leer a la vez.

**Ejemplo:**
```bash
mvar 2 2 &    # 2 escritores, 2 lectores, en background
```

**Comportamiento esperado:** Los escritores y lectores se coordinan correctamente, mostrando letras balanceadas entre todos los escritores.

### 5. Drivers

#### Driver de Teclado

- ✅ Interrupciones de teclado manejadas correctamente.
- ✅ Soporte para teclas especiales (Shift, Ctrl, CapsLock).
- ✅ Integración con TTY para entrada de caracteres.

#### Driver de Video

- ✅ Modo texto funcional.
- ✅ Soporte para colores y posicionamiento.

#### System Calls

Las system calls permiten la interacción entre kernel y user space:
- `sys_read(int fd, void* buf, int count)`: Leer desde file descriptor.
- `sys_write(int fd, const void* buf, int count)`: Escribir a file descriptor.
- `sys_close(int fd)`: Cerrar file descriptor.
- `sys_dup2(int old_fd, int new_fd)`: Duplicar file descriptor.

### 6. User Space Applications

#### Shell (sh)

La shell implementa:

- ✅ Ejecución de procesos en foreground y background.
- ✅ Pipes entre procesos (`|`).
- ✅ Soporte para Ctrl+D (EOF).
- ✅ Soporte para Ctrl+C (mata proceso en foreground).

#### Comandos Implementados

Todos los comandos se ejecutan como **procesos de usuario** (no built-ins), lo que permite:

- ✅ Ejecución en background (con `&`).
- ✅ Compatibilidad con pipes.
- ✅ Lectura desde stdin y escritura a stdout.

**Comandos disponibles:**

1. **`help`**: Muestra ayuda sobre todos los comandos disponibles.

2. **`mem [-v]`**: Muestra estadísticas de memoria:
   ```bash
   mem              # Estadísticas básicas
   mem -v           # Estadísticas detalladas (verbose)
   ```
   Muestra: heap total, usado, libre, bloques libres, fragmentación, etc.

3. **`ps`**: Lista todos los procesos con información detallada:
   - PID, prioridad, estado, ticks restantes, foreground/background
   - Stack pointer, base pointer, nombre del proceso

4. **`loop [-p priority]`**: Proceso de test que hace loop infinito:
   ```bash
   loop             # Loop con prioridad por defecto
   loop -p 1        # Loop con prioridad 1 (alta)
   ```

5. **`kill <pid>`**: Mata un proceso:
   ```bash
   kill 5
   ```

6. **`nice <pid> <priority>`**: Cambia prioridad de un proceso:
   ```bash
   nice 5 3    # Cambiar PID 5 a prioridad 3
   ```

7. **`block <pid>`**: Bloquea un proceso.

8. **`cat`**: Lee desde stdin y escribe a stdout.
   ```bash
   cat
   echo "hola" | cat    # Lee desde pipe
   ```

9. **`wc`**: Cuenta líneas desde stdin.
   ```bash
   echo "linea1\nlinea2" | wc    # Salida: 2
   ```

10. **`filter`**: Elimina vocales desde stdin.
    ```bash
    echo "hola mundo" | filter    # Salida: "hl mnd"
    ```

11. **`mvar <writers> <readers>`**: Comando MVar para IPC coordinado.
    ```bash
    mvar 2 2      # 2 escritores, 2 lectores
    mvar 2 2 &    # En background
    ```

12. **`test_mm [bytes]`**: Test del gestor de memoria.
    ```bash
    test_mm              # Default: 100000000 bytes
    test_mm 50000000     # 50MB
    ```

13. **`test_processes [n]`**: Test de gestión de procesos.
    ```bash
    test_processes       # Default: 10 procesos
    test_processes 20    # 20 procesos
    ```

14. **`test_priority [n]`**: Demostración de scheduling.
    ```bash
    test_priority        # Default: 5 procesos
    test_priority 10     # 10 procesos
    ```

15. **`test_no_synchro [n]`**: Test sin sincronización (race condition).
    ```bash
    test_no_synchro 5
    ```

16. **`test_synchro [n] [use_sem]`**: Test sincronizado con semáforos.
    ```bash
    test_synchro 5           # Con semáforos (default)
    test_synchro 5 1         # Con semáforos (explícito)
    test_synchro 5 0         # Sin semáforos
    ```

## ⌨️ Caracteres Especiales y Atajos

### Pipes (`|`)

Conecta la salida de un comando a la entrada de otro:

```bash
echo "hola mundo" | wc           # Cuenta líneas
echo "abracadabra" | filter      # Elimina vocales
echo "test" | cat | wc           # Chain de pipes
```

**Limitación actual:** La shell soporta pipes simples de dos comandos. Pipes múltiples pueden requerir expansión futura.

### Background (`&`)

Ejecuta un comando en background (no bloquea la shell):

```bash
loop -p 3 &              # Loop en background
test_mm 50000000 &       # Test de memoria en background
mvar 2 2 &               # MVar en background
```

Cuando un comando termina en background, la shell continúa aceptando nuevos comandos inmediatamente.

### Ctrl+C (Interrupción)

- **Comportamiento**: Mata el proceso que tiene control del foreground.
- **Implementación**: Envía señal SIGINT al proceso foreground actual.
- **Uso**: Útil para terminar procesos que se quedaron en loop o bloquearon.

**Ejemplo:**
```bash
loop -p 1           # Proceso en foreground
# Presionar Ctrl+C mata el loop
```

### Ctrl+D (EOF)

- **Comportamiento**: Envía End-of-File (EOF) a la entrada estándar.
- **Implementación**: Marca la TTY como EOF y desbloquea procesos esperando leer.
- **Uso**: Útil para terminar entrada interactiva en comandos como `cat`.

**Ejemplo:**
```bash
cat                 # Espera entrada
# Escribir texto...
# Presionar Ctrl+D termina la entrada
```

## 📋 Ejemplos de Uso Reales

### Ejemplo 1: Testing de Memoria

```bash
# Compilar con Buddy System
make buddy

# Ejecutar test de memoria en foreground
test_mm 50000000

# Ejecutar en background
test_mm 100000000 &

# Consultar estadísticas mientras corre
mem -v
```

### Ejemplo 2: Testing de Procesos y Prioridades

```bash
# Crear loops con diferentes prioridades
loop -p 1 &    # Alta prioridad
loop -p 3 &    # Baja prioridad

# Ver procesos
ps

# Cambiar prioridad
nice 5 1      # Cambiar PID 5 a prioridad 1

# Matar procesos
kill 5
kill 6
```

### Ejemplo 3: Testing de Sincronización

```bash
# Test sin sincronización (resultado variable)
test_no_synchro 10 &

# Test con sincronización (resultado siempre 0)
test_synchro 10 &

# Verificar con ps que los procesos estén bloqueándose correctamente
ps
```

### Ejemplo 4: Uso de Pipes

```bash
# Pipeline simple
echo "hola mundo" | wc

# Pipeline con filter
echo "abracadabra" | filter

# Pipeline complejo (requiere expansión futura)
# echo "test" | cat | wc
```

### Ejemplo 5: MVar con Múltiples Escritores y Lectores

```bash
# 2 escritores, 2 lectores en background
mvar 2 2 &

# Ver procesos MVar
ps

# Los escritores escriben 'A' y 'B' alternadamente
# Los lectores consumen y muestran en color
```

### Ejemplo 6: Workflow Completo

```bash
# 1. Ver procesos
ps

# 2. Crear proceso de test
loop -p 2 &

# 3. Consultar memoria
mem

# 4. Ver procesos actualizados
ps

# 5. Cambiar prioridad
nice <pid> 1

# 6. Matar proceso
kill <pid>
```

## ⚠️ Requerimientos Faltantes o Limitaciones

### Limitaciones Conocidas

1. **Pipes múltiples**: La shell actualmente soporta pipes de dos comandos. Chains de más comandos pueden requerir expansión.

2. ~~**Wait_children**~~ ✅ **RESUELTO**: Syscall `wait_children` implementada como syscall 47. Espera a cualquier proceso hijo.

3. **PVS-Studio**: El análisis con PVS-Studio debe ejecutarse manualmente. Para ejecutarlo:
   ```bash
   # Instalar PVS-Studio (si no está instalado)
   # Ejecutar análisis en el directorio Kernel
   cd Kernel
   pvs-studio-analyzer trace -- make all
   pvs-studio-analyzer analyze
   plog-converter -a 'GA:1,2;64:1;MISRA:1,2;OP:1,2,3;CS' -t fullhtml -o report report.plog
   ```
   O alternativamente, usar el target del Makefile si está disponible:
   ```bash
   make pvs-analyze  # (pendiente de implementación)
   ```

4. **Tamaño de Heap**: El heap está limitado por el tamaño de memoria asignado al sistema (típicamente 512MB en QEMU).

5. **Procesos Máximos**: Hay un límite de `MAX_PROCS` procesos simultáneos (configurable, típicamente 128).

6. **Fragmentación de Memoria**: First Fit puede sufrir fragmentación externa con el tiempo. Buddy System reduce esto pero tiene fragmentación interna.

### Mejoras Futuras Sugeridas

- Soporte para pipes de múltiples comandos en la shell.
- Implementación de más comandos Unix-like (ls, grep, etc.).
- Soporte para redirección de entrada/salida (`<`, `>`).
- Historial de comandos mejorado.
- Auto-completado de comandos.
- Mejora en el manejo de señales.

## 📝 Notas de Desarrollo

### Estructura del Proyecto

```
TP2-Kernel/
├── Bootloader/          # Código del bootloader (Pure64, BMFS)
├── Kernel/              # Código del kernel
│   ├── drivers/         # Drivers (teclado, video, sonido)
│   ├── interrupt/       # Manejo de interrupciones y syscalls
│   ├── asm/             # Código assembly
│   └── include/         # Headers del kernel
├── Userland/            # Aplicaciones de usuario
│   └── SampleCodeModule/ # Shell y comandos
├── Image/               # Archivos de imagen generados
└── Toolchain/           # Herramientas de compilación
```

### Memory Managers

El cambio entre First Fit y Buddy System se hace en tiempo de compilación mediante el flag `USE_BUDDY_SYSTEM`. Ambos comparten la misma interfaz pública para facilitar el intercambio.

### Compilación sin Warnings

El proyecto compila con `-Wall -Wextra -Werror`, por lo que cualquier warning se trata como error. Esto garantiza código limpio y sin advertencias del compilador.

## 📄 Licencia

Ver `License.txt` para más información.

## 👥 Autores

Desarrollado como parte del trabajo práctico de Sistemas Operativos.

---

**Nota**: Este README documenta todas las características implementadas según los requerimientos del TP2. Para más información técnica, consultar el código fuente y los headers en `Kernel/include/` y `Userland/SampleCodeModule/include/`.

