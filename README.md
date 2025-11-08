# TP2 - Kernel Operating System

Sistema operativo desarrollado sobre x64BareBones con gestión de memoria, multitasking, sincronización e IPC.

---

## Instrucciones de Compilación y Ejecución

### Requisitos
- **Docker**: `agodio/itba-so-multi-platform:3.0`
- **QEMU**: Para ejecutar el kernel

### Compilación

```bash
# Inicializar entorno Docker
make docker
cd Toolchain
make clean all
cd ..

# Compilar con First Fit (default)
make clean all

# O compilar con Buddy System
make clean buddy
```

### Ejecución

```bash
./run.sh
```

---

## Instrucciones de Replicación

### Comandos de Usuario

Todos los comandos se ejecutan como procesos independientes, soportan pipes (`|`) y ejecución en background (`&`).

#### Comandos Básicos

- **`help`** / **`ls`**: Lista todos los comandos disponibles.

- **`mem [-v]`**: Muestra estadísticas del gestor de memoria.
  - Sin parámetros: estadísticas básicas (heap total, usado, libre)
  - `-v`: estadísticas detalladas (bloques, fragmentación)

- **`ps`**: Lista procesos activos (PID, prioridad, estado, ticks, stack/base pointer, nombre).

- **`cat`**: Lee stdin y escribe a stdout. 

- **`wc`**: Cuenta líneas de stdin.

- **`filter`**: Elimina vocales de stdin.

#### Gestión de Procesos

- **`loop [-p priority]`**: Crea proceso de loop infinito.
  - Sin parámetros: prioridad por defecto
  - `-p N`: prioridad N (0-3, mayor = más CPU)

- **`kill <pid>`**: Termina proceso por PID.

- **`nice <pid> <priority>`**: Cambia prioridad de proceso (0-3).

- **`block <pid>`**: Bloquea proceso.

- **`unblock <pid>`**: Desbloquea proceso.

- **`yield`**: Cede CPU voluntariamente.

- **`waitpid [pid|-1]`**: Espera terminación de proceso hijo.
  - Sin parámetros: espera cualquier hijo
  - `pid`: espera proceso específico
  - `-1`: espera cualquier hijo

#### Tests del Sistema

- **`test_mm [bytes]`**: Prueba el gestor de memoria.
  - Parámetro: cantidad de bytes a asignar (default: 100000000)
  - Realiza ciclos de malloc/free en bloques de tamaño aleatorio
  - Imprime "test_mm OK" si no hay errores, "test_mm ERROR" en caso contrario

- **`test_processes [n]`**: Prueba creación/destrucción de procesos.
  - Parámetro: cantidad de procesos a crear (default: 10)
  - Crea N procesos, los bloquea/mata aleatoriamente, los desbloquea
  - Imprime estado de cada operación (create/kill/block/unblock)

- **`test_priority [limit]`**: Demuestra scheduling con prioridades.
  - Parámetro: cantidad de iteraciones por proceso (default: 1500)
  - **Fase 1**: Crea 3 procesos con prioridad MEDIUM (todos iguales), avanzan uniformemente
  - **Fase 2**: Cambia prioridades a LOW(0), MEDIUM(1), HIGH(2), el de mayor prioridad avanza más rápido
  - Cada proceso cuenta hasta el límite e imprime progreso cada 10%
  - Demuestra que prioridad alta obtiene más CPU time

- **`test_no_synchro [n]`**: Demuestra race conditions.
  - Parámetro: cantidad de pares inc/dec (default: 5)
  - Crea N procesos que incrementan y N que decrementan variable global
  - **Sin sincronización**: resultado final != 0 (race condition)
  - Imprime valor final

- **`test_synchro [n] [use_sem]`**: Demuestra sincronización con semáforos.
  - Parámetro 1: cantidad de pares inc/dec (default: 5)
  - Parámetro 2: 1=con semáforos, 0=sin (default: 1)
  - **Con semáforos**: resultado final = 0 (correcto)
  - Imprime valor final

#### Aplicación Avanzada

- **`mvar <writers> <readers>`**: Problema de lectores/escritores.
  - Parámetro 1: cantidad de escritores
  - Parámetro 2: cantidad de lectores
  - Simula MVar de Haskell con sincronización
  - Escritores escriben caracteres ('A', 'B', 'C'...) con delay aleatorio
  - Lectores consumen y muestran valores con identificador de color
  - Ejemplo: `mvar 2 3` → 2 escritores, 3 lectores

### Caracteres Especiales

#### Pipes (`|`)
Conecta stdout de un comando con stdin del siguiente:
```bash
ps | filter              # Lista procesos sin vocales
help | wc                # Cuenta líneas del help
```

**Limitación**: Comandos infinitos (`loop`, `mvar`) no funcionan bien con pipes porque nunca envían EOF.

#### Background (`&`)
Ejecuta comando en background (no bloquea la shell):
```bash
loop -p 2 &              # Loop en background
test_mm 50000000 &       # Test en background
```

### Atajos de Teclado

- **Ctrl+C**: Interrumpe y mata proceso en foreground.

- **Ctrl+D**: Envía EOF (End-of-File) a stdin, termina entrada interactiva (ej: `cat`).

### Ejemplos de Uso

#### Ejemplo 1: Testing de Memoria
```bash
# Ver estado actual
mem

# Test de memoria en foreground
test_mm 50000000

# Test en background + consultar stats
test_mm 100000000 &
mem -v
```

#### Ejemplo 2: Scheduling y Prioridades
```bash
# Crear procesos con diferentes prioridades
loop -p 1 &
loop -p 3 &

# Ver procesos
ps

# Cambiar prioridad
nice 5 0

# Limpiar
kill 5
kill 6
```

#### Ejemplo 3: Sincronización
```bash
# Demostrar race condition
test_no_synchro 10

# Demostrar corrección con semáforos
test_synchro 10

# Ver diferencia en resultados finales
```

#### Ejemplo 4: Pipes
```bash
# Filtrar salida de comandos
ps | filter
help | wc
mem | wc

# Procesar entrada del usuario
cat | filter
```

#### Ejemplo 5: Lectores/Escritores
```bash
# Ejecutar mvar
mvar 3 2 &

# Ver procesos creados
ps

# Cambiar prioridad de un escritor
nice <pid> 4

# Matar un lector para ver acumulación
kill <pid>
```

---

## Requerimientos Faltantes o Parcialmente Implementados

**Ninguno.** Todos los requisitos obligatorios del enunciado están completamente implementados:

- Gestión de memoria (First Fit y Buddy System)
- Procesos y scheduling con prioridades
- Semáforos sin busy waiting
- Pipes unidireccionales bloqueantes
- Todos los tests funcionando en foreground y background
- Shell con pipes, background, Ctrl+C y Ctrl+D
- Todos los comandos implementados como procesos de usuario

---

## Limitaciones

### Pipes
- **Pipes múltiples**: La shell soporta pipes simples (`cmd1 | cmd2`). Cadenas largas (`cmd1 | cmd2 | cmd3`) requieren implementación.
- **Procesos infinitos con pipes**: Comandos como `loop`, `mvar` y `test_mm` no funcionan bien con pipes porque nunca terminan ni envían EOF, es decir, no anda bien si hacemos loop | filter.
- **Colores**: Los colores no se preservan en pipes - la salida es texto plano.

### Sistema
- **Señales**: No hay implementación completa de señales (solo Ctrl+C básico).

---

## Uso de Inteligencia Artificial

Durante el desarrollo se utilizó IA (GitHub Copilot y Codex) para:
- Consultas puntuales sobre sintaxis de x86-64 assembly
- Asistencia en la redacción de documentación
- Debugging de race conditions específicas

---

**Desarrollado sobre x64BareBones** | Compilación obligatoria con Docker `agodio/itba-so-multi-platform:3.0`
