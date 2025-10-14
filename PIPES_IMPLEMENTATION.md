# Pipes Implementation (Hito 5)

## Overview
This document describes the complete implementation of pipes with blocking I/O for TP2-Kernel.

## Architecture

### 1. Core Data Structures

#### Pipe Structure (`kpipe_t`)
```c
typedef struct {
    char name[PIPE_NAME_MAX];
    char buf[PIPE_CAP];          // 4096 byte ring buffer
    int r, w, size;              // read pos, write pos, current size
    int readers, writers;        // reference counts
    pcb_t *r_head, *r_tail;     // blocked readers queue
    pcb_t *w_head, *w_tail;     // blocked writers queue
} kpipe_t;
```

#### File Descriptor (`kfd_t`)
```c
typedef struct {
    fd_type_t type;              // FD_TTY or FD_PIPE
    const kfd_ops_t *ops;        // vtable for operations
    void *ptr;                   // pointer to resource (kpipe_t*)
    int can_read, can_write;     // permission flags
} kfd_t;
```

### 2. Syscalls Added

| Number | Name | Description |
|--------|------|-------------|
| 36 | `sys_pipe_open` | Open or create a named pipe |
| 37 | `sys_pipe_close` | Close a pipe reference |
| 38 | `sys_pipe_read` | Read from a pipe (blocking) |
| 39 | `sys_pipe_write` | Write to a pipe (blocking) |
| 40 | `sys_pipe_unlink` | Remove pipe from hash table |
| 41 | `sys_read` | Generic FD read |
| 42 | `sys_write` | Generic FD write |
| 43 | `sys_close` | Generic FD close |

### 3. Files Created/Modified

#### New Files
- `Kernel/include/pipe.h` - Pipe API and structures
- `Kernel/pipe.c` - Pipe implementation (500+ lines)
- `Kernel/include/fd.h` - File descriptor abstraction
- `Kernel/fd.c` - FD table management
- `Kernel/pipe_fd.c` - Pipe FD operations vtable
- `Userland/SampleCodeModule/cat.c` - Cat command
- `Userland/SampleCodeModule/wc.c` - Word count command
- `Userland/SampleCodeModule/filter.c` - Vowel filter command

#### Modified Files
- `Kernel/include/syscalls.h` - Added pipe syscalls
- `Kernel/syscalls.c` - Implemented pipe syscalls
- `Kernel/kernel.c` - Added `fd_init()` call
- `Kernel/interrupt/sysCallDispatcher.c` - Wired syscalls 36-43
- `Userland/SampleCodeModule/include/sys_calls.h` - Added userland declarations
- `Userland/SampleCodeModule/asm/sys_calls.asm` - Added syscall wrappers
- `Userland/SampleCodeModule/kitty.c` - Added cat, wc, filter commands

## Implementation Details

### Ring Buffer Operations

#### Read (`kpipe_read`)
```c
while (p->size == 0 && p->writers > 0) {
    // Block current process
    current->state = BLOCKED;
    current->ticks_left = 0;
    // Add to readers wait queue
    enqueue_to_pipe_wait(&p->r_head, &p->r_tail, current);
    irq_restore(flags);
    sched_force_yield();
    flags = irq_save();
}

// EOF if no writers and buffer empty
if (p->size == 0) {
    return 0;
}

// Read min(n, size) bytes
while (copied < n && p->size > 0) {
    buf[copied++] = p->buf[p->r];
    p->r = (p->r + 1) % PIPE_CAP;
    p->size--;
}

// Wake one writer if buffer has space
if (p->size < PIPE_CAP && p->w_head != NULL) {
    wake_one(&p->w_head, &p->w_tail);
}
```

#### Write (`kpipe_write`)
```c
// EPIPE if no readers
if (p->readers == 0) {
    return -1;
}

while (p->size == PIPE_CAP && p->readers > 0) {
    // Block current process
    current->state = BLOCKED;
    current->ticks_left = 0;
    // Add to writers wait queue
    enqueue_to_pipe_wait(&p->w_head, &p->w_tail, current);
    irq_restore(flags);
    sched_force_yield();
    flags = irq_save();
}

// Write min(n, PIPE_CAP - size) bytes
while (copied < n && p->size < PIPE_CAP) {
    p->buf[p->w] = buf[copied++];
    p->w = (p->w + 1) % PIPE_CAP;
    p->size++;
}

// Wake one reader if buffer has data
if (p->size > 0 && p->r_head != NULL) {
    wake_one(&p->r_head, &p->r_tail);
}
```

### Critical Section Pattern

Following the "decide to block inside lock, sleep outside" pattern established in the semaphore fix:

1. **Inside critical section** (interrupts disabled):
   - Check condition (e.g., buffer empty)
   - Mark process as BLOCKED
   - Set ticks_left = 0
   - Add to wait queue

2. **Outside critical section** (interrupts enabled):
   - Call `sched_force_yield()` to switch processes

This prevents lost wakeups where a process could be unblocked before it's fully marked as blocked.

### Hash Table Management

Pipes are stored in a hash table with 16 buckets:
```c
static kpipe_t *pipe_table[PIPE_HASH_BUCKETS];

static unsigned hash_pipe_name(const char *name) {
    unsigned h = 0;
    while (*name) {
        h = h * 31 + (*name++);
    }
    return h % PIPE_HASH_BUCKETS;
}
```

### File Descriptor Table

Global table with 64 slots (TODO: migrate to per-process):
- FD 0: stdin (TTY)
- FD 1: stdout (TTY)
- FD 2: stderr (TTY)
- FD 3-63: Available for pipes

### Reference Counting

Pipes track `readers` and `writers` counters:
- Incremented in `kpipe_open` based on flags (PIPE_READ/PIPE_WRITE)
- Decremented in `kpipe_close`
- Pipe destroyed when both reach 0
- Write returns -1 (EPIPE) when readers == 0
- Read returns 0 (EOF) when writers == 0 and buffer empty

## Testing

### Test Commands

1. **cat**: Reads stdin, writes stdout
   ```
   cat
   [type some text]
   [press Ctrl+D to send EOF]
   ```

2. **wc**: Counts lines
   ```
   wc
   line 1
   line 2
   line 3
   [Ctrl+D]
   3
   ```

3. **filter**: Removes vowels
   ```
   filter
   hello world
   hll wrld
   ```

### Expected Pipe Usage (Shell with pipes - future work)
```
echo "hola" | wc      # Should output: 1
cat | filter           # Reads stdin, filters vowels
```

## Key Design Decisions

1. **4KB Ring Buffer**: Efficient for most use cases, matches typical page size
2. **Blocking I/O**: Aligns with Unix semantics, simpler than async
3. **Wake-one**: Only one waiting process woken per event (prevents thundering herd)
4. **Partial Transfers**: Read/write can return less than requested (like POSIX)
5. **Hash Table**: O(1) lookup by name, 16 buckets sufficient for most cases
6. **Global FD Table**: Simpler implementation, TODO migrate to per-process

## Known Limitations

1. **Global FD Table**: Should be per-process for proper isolation
2. **No Pipe Operator**: Shell doesn't support `|` syntax yet (needs parser)
3. **Named Pipes Only**: No anonymous pipes (pipe2 syscall)
4. **Fixed Buffer Size**: 4KB hardcoded, not configurable
5. **No NONBLOCK Flag**: All operations are blocking

## Future Improvements

1. Per-process file descriptor tables
2. Shell parser with `|` operator support
3. Anonymous pipes (pipe2 syscall)
4. O_NONBLOCK flag support
5. Select/poll for multiplexing
6. Pipe size configuration
7. Better error messages

## Verification

Compile the kernel with:
```bash
make clean && make all
```

Expected files to be compiled:
- `Kernel/pipe.o`
- `Kernel/fd.o`
- `Kernel/pipe_fd.o`
- `Userland/SampleCodeModule/cat.o`
- `Userland/SampleCodeModule/wc.o`
- `Userland/SampleCodeModule/filter.o`

## References

- Original requirement: "echo 'hola' | wc y cat | filter"
- Semaphore fix pattern: SEMAPHORE_FIX.md
- Blocking I/O: "decide to block inside lock, sleep outside"
- Ring buffer: Wraparound with modulo PIPE_CAP

---
*Document created: October 14, 2025*
*Implementation: Hito 5 - Pipes with Blocking I/O*
