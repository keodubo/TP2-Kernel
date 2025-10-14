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


Author: Rodrigo Rearden (RowDaBoat)
Collaborator: Augusto Nizzo McIntosh
