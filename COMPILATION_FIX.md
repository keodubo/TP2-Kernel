# Compilation Fix - Missing stddef.h

## Issue
The pipe implementation files were missing `#include <stddef.h>`, which is required for the `NULL` macro in C99.

## Error Message
```
fd.c:11:27: error: 'NULL' undeclared (first use in this function)
   11 |         fd_table[i].ops = NULL;
      |                           ^~~~
fd.c:3:1: note: 'NULL' is defined in header '<stddef.h>'; did you forget to '#include <stddef.h>'?
```

## Files Fixed

### 1. Kernel/fd.c
Added `#include <stddef.h>` at the top of the file.

### 2. Kernel/pipe.c
Added `#include <stddef.h>` at the top of the file.

### 3. Kernel/pipe_fd.c
Added `#include <stddef.h>` at the top of the file.

## Why This Was Needed

In C99 standard, `NULL` is defined in `<stddef.h>`. While some compilers may implicitly include this header through other includes, the strict `-ffreestanding` compilation flag used in the kernel build process doesn't provide standard library headers automatically.

The kernel Makefile uses:
```
GCCFLAGS=-m64 -fno-exceptions ... -ffreestanding -nostdlib -fno-common -std=c99 -g
```

The `-ffreestanding` flag indicates that the code doesn't rely on a hosted environment, so we must explicitly include even basic headers like `<stddef.h>`.

## Additional Fix: Type Signedness

### Issue
The compiler flagged sign comparison warnings in `pipe.c`:
```
pipe.c:277:36: error: comparison of integer expressions of different signedness: 'uint32_t' {aka 'unsigned int'} and 'int' [-Werror=sign-compare]
```

### Root Cause
The pipe structure uses `uint32_t size` (unsigned) but the read/write functions use `int n` (signed) for byte counts. Direct comparisons between signed and unsigned integers trigger `-Werror=sign-compare`.

### Solution in pipe.c

#### kpipe_read function
Changed:
```c
int to_read = (p->size < (n - total_read)) ? p->size : (n - total_read);
```

To:
```c
int remaining = n - total_read;
int to_read = ((int)p->size < remaining) ? (int)p->size : remaining;
```

#### kpipe_write function
Changed:
```c
int space = PIPE_CAP - p->size;
int to_write = (space < (n - total_written)) ? space : (n - total_written);
```

To:
```c
int space = (int)(PIPE_CAP - p->size);
int remaining = n - total_written;
int to_write = (space < remaining) ? space : remaining;
```

This ensures all comparisons are between signed integers, avoiding the warning.

## Additional Fix: Syscall Dispatcher Type Mismatch

### Issue
The syscall dispatcher was passing wrong argument types:
```
interrupt/sysCallDispatcher.c:307:31: error: passing argument 1 of 'sys_pipe_close' makes integer from pointer without a cast [-Werror=int-conversion]
  307 |         return sys_pipe_close((const char*)rdi);
```

### Root Cause
Confusion between pipe operations that take **names** vs those that take **file descriptors**:
- `sys_pipe_open(name, flags)` - takes string name
- `sys_pipe_close(fd)` - takes int file descriptor
- `sys_pipe_read(fd, buf, n)` - takes int file descriptor
- `sys_pipe_write(fd, buf, n)` - takes int file descriptor
- `sys_pipe_unlink(name)` - takes string name

### Solution in sysCallDispatcher.c

Changed cases 37-39 and 41-42 to cast `rdi` as `(int)` instead of `(const char*)`:

```c
case 37:
    return sys_pipe_close((int)rdi);  // was: (const char*)rdi
case 38:
    return sys_pipe_read((int)rdi, (void*)rsi, (int)rdx);  // was: (const char*)rdi, (size_t)rdx
case 39:
    return sys_pipe_write((int)rdi, (const void*)rsi, (int)rdx);  // was: (const char*)rdi, (size_t)rdx
```

Also fixed cases 41-42 to use `(int)` for byte count instead of `(size_t)`.

## Additional Fix: Userland Compilation Issues

### Issue 1: Undeclared cmd functions in kitty.c
```
kitty.c:280:9: error: 'cmd_cat' undeclared here (not in a function)
  280 |         cmd_cat,
```

### Root Cause
The `commands_ptr` array referenced `cmd_cat`, `cmd_wc`, and `cmd_filter` functions before they were declared. In C99, functions must be declared before use.

### Solution
Added forward declarations for all cmd functions before the `commands_ptr` array:
```c
// Forward declarations for cmd functions
void cmd_undefined(void);
void cmd_help(void);
// ... (all cmd functions)
void cmd_cat(void);
void cmd_wc(void);
void cmd_filter(void);
```

### Issue 2: putchar undeclared in wc.c
```
wc.c:7:9: error: implicit declaration of function 'putchar' [-Werror=implicit-function-declaration]
```

### Root Cause
The `print_number` function used `putchar()` which wasn't declared in the userland stdio.h header.

### Solution
Simplified wc.c to use `printf()` which is already available:
```c
// Changed from:
print_number(lines);
putchar('\n');

// To:
printf("%d\n", lines);
```

### Issue 3: Array size overflow in kitty.h
```
kitty.c:309:9: error: excess elements in array initializer [-Werror]
  309 |         cmd_filter
```

### Root Cause
The `commands_ptr` array was defined with size `MAX_ARGS = 24`, but we added 3 new commands (cat, wc, filter), bringing the total to 25 commands. The array initialization had more elements than the declared size.

### Solution
Increased `MAX_ARGS` in kitty.h:
```c
// Changed from:
#define MAX_ARGS 24

// To:
#define MAX_ARGS 32
```

This gives room for future commands while fixing the immediate overflow.

## Verification

After applying these fixes, the kernel should compile successfully:
```bash
make clean && make all
```

All files now:
1. Properly include `<stddef.h>` before using `NULL`
2. Use explicit casts to avoid signed/unsigned comparison warnings

---
*Fix applied: October 14, 2025*
