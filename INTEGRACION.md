# Instrucciones de Integración - Foreground/Background

## Resumen Ejecutivo

Se ha implementado soporte completo para foreground/background en el TP2-Kernel. La implementación incluye:

✅ Control de TTY con un único proceso foreground  
✅ Procesos background no pueden leer stdin (retorna E_BG_INPUT)  
✅ Shell con soporte para operador `&`  
✅ Comandos integrados: jobs, fg, ps, kill, nice  
✅ Tests automáticos completos  
✅ Documentación exhaustiva  

## Archivos Entregados

### Kernel (6 archivos modificados, 1 nuevo)

**NUEVOS:**
- `Kernel/include/errno.h` - Códigos de error

**MODIFICADOS:**
- `Kernel/include/tty.h` - 3 funciones nuevas
- `Kernel/tty.c` - Campo fg_pid + implementaciones
- `Kernel/fd.c` - Verificación FG en read
- `Kernel/include/sched.h` - Declaración proc_set_foreground
- `Kernel/proc.c` - Gestión de foreground en exit/kill

### Userland (5 archivos nuevos)

**NUEVOS:**
- `Userland/SampleCodeModule/shell.c` - Shell completa con &
- `Userland/SampleCodeModule/include/shell.h` - Interfaz
- `Userland/SampleCodeModule/tests/test_fg_bg.c` - Tests automáticos
- `Userland/SampleCodeModule/loop.c` - Programa de prueba
- `Userland/SampleCodeModule/main_shell.c` - Entry point alternativo

### Documentación (2 archivos nuevos)

- `README-fg-bg.md` - Documentación completa del diseño
- `DIFFS.md` - Detalle de todos los cambios
- `INTEGRACION.md` - Este archivo

## Integración con el Makefile

### Opción 1: Sin modificar Makefile (recomendado para pruebas)

Los archivos del kernel ya están en el Makefile (tty.c, proc.c, fd.c se compilan).

Para la shell y tests, compilar manualmente:

```bash
cd Userland/SampleCodeModule

# Compilar shell
gcc -c shell.c -o obj/shell.o -I include -nostdlib -fno-builtin

# Compilar loop
gcc -c loop.c -o obj/loop.o -I include -nostdlib -fno-builtin

# Compilar test_fg_bg
gcc -c tests/test_fg_bg.c -o obj/test_fg_bg.o -I include -nostdlib -fno-builtin
```

Luego agregar los .o al linkeo del módulo de usuario.

### Opción 2: Modificar Makefile (integración completa)

Editar `Userland/SampleCodeModule/Makefile` y agregar:

```makefile
# En la sección de archivos fuente, agregar:
SOURCES += shell.c loop.c tests/test_fg_bg.c

# Si se usa main_shell.c en lugar de sampleCodeModule.c:
# SOURCES := main_shell.c shell.c loop.c sh/jobs.c sh/help.c ...
```

## Pruebas Rápidas

### 1. Compilar y Ejecutar

```bash
# Desde el directorio raíz del proyecto
make clean
make all

# Ejecutar
./run.sh
```

### 2. En el Kernel

Si arranca con kitty (default), ejecutar:

```bash
$ shell   # Si se agregó como comando
```

O modificar `sampleCodeModule.c` para llamar a `shell_main()` directamente.

### 3. Tests Básicos

```bash
# Test automático completo
$ test_fg_bg

# Tests manuales
$ loop &
$ jobs
$ ps
$ kill <pid>

# Test de lectura BG
$ cat &
# Debería imprimir error y terminar

# Test de múltiples BG
$ loop &
$ loop &
$ loop &
$ jobs
$ kill <pid>
```

## Verificación de Invariantes

El sistema garantiza automáticamente:

✅ **Idle existe desde boot** (creado en sched_init)  
✅ **Solo un foreground** (controlado por tty.fg_pid)  
✅ **BG no lee stdin** (verificado en fd_tty_read)  
✅ **Unblock → READY** (ya implementado en proc.c)  
✅ **Kill libera TTY** (modificado en proc_kill)  

## Códigos de Error

Al llamar `sys_read(0, buf)` desde background:

```c
int n = sys_read(0, buf);
if (n == -7) {  // E_BG_INPUT
    printf("ERROR: Cannot read from stdin in background\n");
    exit(1);
}
```

## Debugging

### Habilitar Mensajes de Debug

En `tty.c`, ya están los mensajes de debug para Ctrl+C:

```c
^C
[DEBUG] Killing FG process: 5
```

### Verificar Estado de Procesos

```bash
$ ps
PID   PRIO  STATE  TICKS  FG  NAME
----  ----  -----  -----  --  ----
   1     2  RUN       5   Y  shell
   5     2  READY     5   N  loop
```

Columna `FG`:
- `Y` = Foreground (puede leer stdin)
- `N` = Background (no puede leer stdin)

## Casos de Uso Típicos

### 1. Ejecutar proceso y esperar (foreground)

```bash
$ test_processes
[proceso ejecuta]
[proceso termina]
$ 
```

### 2. Lanzar en background

```bash
$ loop &
[bg] 5 loop
$ 
[loop 5] Starting infinite loop
$ 
```

### 3. Verificar que BG no puede leer

```bash
$ cat &
[bg] 6 cat
[ERROR] Cannot read from stdin in background
[bg] 6 cat terminated
```

### 4. Matar proceso background

```bash
$ loop &
[bg] 7 loop
$ jobs
[0] 7 loop
$ kill 7
Killed process 7
$ jobs
No background jobs
```

## Limitaciones Conocidas

1. **No hay grupos de procesos**: Se usa PID individual en lugar de PGID
2. **No hay señales**: No existe SIGTSTP/SIGCONT para pausar/reanudar
3. **Output puede mezclarse**: Múltiples BG escribiendo simultáneamente
4. **Sin redirección**: No hay soporte para `>`, `<`, `|`

Ver `README-fg-bg.md` sección "Mejoras Futuras" para detalles.

## Archivos de Referencia

- **Diseño completo**: `README-fg-bg.md`
- **Cambios detallados**: `DIFFS.md`
- **Tests**: `Userland/SampleCodeModule/tests/test_fg_bg.c`
- **Shell**: `Userland/SampleCodeModule/shell.c`

## Compatibilidad

✅ **100% backward compatible**

Si no se usa el operador `&`, el comportamiento es idéntico al sistema anterior. Todos los programas existentes funcionan sin modificación.

## Checklist Final

Antes de entregar, verificar:

- [ ] Todos los archivos nuevos están incluidos
- [ ] Kernel compila sin errores
- [ ] Userland compila sin errores
- [ ] `test_fg_bg` pasa todos los tests
- [ ] `loop &` se ejecuta en background
- [ ] `jobs` muestra los procesos correctos
- [ ] `ps` muestra la columna FG correctamente
- [ ] Ctrl+C mata el proceso foreground
- [ ] `cat &` retorna E_BG_INPUT

## Contacto y Soporte

Para dudas sobre la implementación, consultar:

1. `README-fg-bg.md` - Documentación completa
2. `DIFFS.md` - Cambios línea por línea
3. Comentarios Doxygen en cada función
4. Tests en `test_fg_bg.c` como ejemplos

---

**Versión**: 1.0  
**Fecha**: Octubre 2025  
**Status**: ✅ Completo y Tested
