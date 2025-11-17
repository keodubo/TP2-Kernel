#include <stdio.h>
#include <sys_calls.h>

// Proceso simple que hace un loop infinito
// Útil para probar ejecución en background, prioridades y kill
// Uso: loop (se debe detener con kill)
void loop_main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    int64_t pid = sys_getpid();

    printf("[loop %d] Starting infinite loop (use kill to stop)\n", (int)pid);

    int count = 0;
    while (1) {
        count++;

        // Imprimir cada 100000 iteraciones para no saturar la pantalla
        if (count % 100000 == 0) {
            printf("[loop %d] Iteration %d\n", (int)pid, count / 100000);
        }

        // Ceder el CPU periódicamente para permitir multitarea
        if (count % 10000 == 0) {
            sys_yield();
        }
    }
}
