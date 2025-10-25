# Memory Stats Audit

| Item | Status | Location(s) | Notes |
| --- | --- | --- | --- |
| A. Shared stats struct | OK | `Kernel/include/mm_stats.h:1`, `Userland/include/mm_stats.h:1` | Added mirrored ABI with buddy order table, freelist snapshot, and optional heap bounds to satisfy kernel↔userland contract. |
| B. Kernel plumbing | OK | `Kernel/first_fit.c:333`, `Kernel/buddy_system.c:368`, `Kernel/memory_manager.c:73`, `Kernel/syscalls.c:78`, `Kernel/interrupt/sysCallDispatcher.c:333`, `Kernel/Makefile:56` | Collectors fill mm_stats under IRQ guard, clamp invariants, syscall copyout wired into dispatcher, and build enables `CONFIG_MM_BUDDY` when buddy flag set. |
| C. Userland `mem` command | OK | `Userland/SampleCodeModule/mem.c:1`, `Userland/SampleCodeModule/include/sys_calls.h:6`, `Userland/SampleCodeModule/asm/sys_calls.asm:164`, `Userland/SampleCodeModule/shell.c:169`, `Userland/SampleCodeModule/kitty.c:323` | New user tool prints summary & verbose views, handles `-v`, validates errors, and CLI wiring (modern shell + legacy kitty prompt) exposes the command. |
| D. Build flags / ABI stability | OK | `Userland/SampleCodeModule/Makefile:13`, `Kernel/Makefile:56` | Userland pulls shared headers via `-I../include`; kernel adds `CONFIG_MM_BUDDY` when buddy allocator is selected without rebuilding userland. |
| E. Edge cases & guards | OK | `Kernel/first_fit.c:355`, `Kernel/buddy_system.c:419`, `Userland/SampleCodeModule/mem.c:55` | Largest block clamped to free total, computed free reconciled, fragmentation prints `N/A` when free is zero, freelist truncation tracked. |
| F. Tests | OK | `run_mem_tests.sh:1` | Headless harness builds both allocators, captures `mem`/`mem -v` output via QEMU+expect, and checks invariants (heap balance, buddy order sum, fragmentation). |
