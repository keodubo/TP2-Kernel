x64BareBones is a basic setup to develop operating systems for the Intel 64 bits architecture.

The final goal of the project is to provide an entry point for a kernel and the possibility to load extra binary modules separated from the main kernel.

Environment setup:
1- Install the following packages before building the Toolchain and Kernel:

nasm qemu gcc make

2- Build the Toolchain

Execute the following commands on the x64BareBones project directory:

  user@linux:$ cd Toolchain
  user@linux:$ make all

3- Build the Kernel

From the x64BareBones project directory run:

  user@linux:$ make all

4- Run the kernel

From the x64BareBones project directory run:

  user@linux:$ ./run.sh


Testing preemptive multitasking
-------------------------------
Inside the shell you can exercise the scheduler and syscalls with the following commands:

- `ps` — list every process with pid, priority, state, remaining ticks and whether it runs in foreground.
  Example output:
    PID 1 PRIO 0 STATE RUN TICKS 5 FG BG idle
- `loop -p 3` — spawn a CPU-bound test process at priority 3. Spawn another with `loop -p 1` to contrast priorities.
- `nice <pid> <prio>` — change the priority of a running loop. For example `nice 5 1` lowers pid 5 to priority 1.
- `yield` — make the current shell process yield immediately (`ps` shows the state flip back to READY).
- `kill <pid>` — terminate a process created with `loop`. `ps` will no longer list the killed pid.
- `test_processes <n>` — launches the stress test that randomly blocks/unblocks/kills `n` worker processes. The test runs in its own process so the shell stays responsive (kill it with `kill <pid>` when you are done).
- `test_sync <n> <use_sem>` — spawns pairs of increment/decrement workers. Run it with `<use_sem>=0` to expose the race condition and with `<use_sem>=1` to verify the synchronised path.
- `test_mm [bytes]` — exercises the memory manager in a separate process. You can override the amount of memory requested (default `100000000`).

Suggested manual test flow:
1. Run `loop -p 3` and `loop -p 1`; observe the higher-priority loop printing more often.
2. Use `ps` to verify both loops are READY/RUNNING with different priorities.
3. Issue `nice <low_pid> 3` and re-run `ps` to confirm the priority change.
4. Use `yield` from the shell to force an immediate context switch to a loop.
5. Finish by `kill <pid>` for each loop and confirm the scheduler falls back to the idle task.

### Kernel Semaphores

- `sys_sem_open(const char *name, unsigned init)` returns a handle (`int`) that represents the semaphore.
- `sys_sem_wait(handle)` decrements the semaphore or blocks the caller (state `BLOCKED`) until a `sys_sem_post` wakes it.
- `sys_sem_post(handle)` wakes exactly one waiter (FIFO order) or increments the count if no-one is waiting.
- `sys_sem_close(handle)` releases the caller's reference. Pair it with `sys_sem_unlink(name)` to remove the semaphore from the namespace once all users have closed it.

Example (userland):

```
int handle = sys_sem_open("mutex", 1);
sys_sem_wait(handle);
/* critical section */
sys_sem_post(handle);
sys_sem_close(handle);
sys_sem_unlink("mutex");
```

### Semaphore demos

- `test_no_synchro <n>` launches `2*n` workers that increment/decrement a shared counter **without** semaphores. The final value varies on each run, evidencing the race condition.
- `test_synchro <n> [use_sem]` runs the synchronised flavour (defaults to using semaphores). With `use_sem=1` the final value is deterministically `0`, demonstrating the absence of busy-waiting and correct wake-up order. With `use_sem=0` it collapses to the unsynchronised behaviour.


Author: Rodrigo Rearden (RowDaBoat)
Collaborator: Augusto Nizzo McIntosh
