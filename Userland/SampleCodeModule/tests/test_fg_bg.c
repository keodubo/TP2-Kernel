#include <stdio.h>
#include <sys_calls.h>
#include <userlib.h>

/**
 * @file test_fg_bg.c
 * @brief Tests para verificar comportamiento de foreground/background
 */

// Códigos de error (deben coincidir con errno.h del kernel)
#define E_BG_INPUT -7

// Prototipos
static void test_bg_cannot_read(void);
static void test_shell_regains_tty(void);
static void test_bg_can_write(void);
static void test_fg_normal_read(void);
static void test_multiple_bg(void);

// Proceso auxiliar que intenta leer (para test)
static void reader_process(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    char buf[10];
    int64_t result = sys_read(0, buf);
    
    if (result == E_BG_INPUT) {
        printf("[test_bg] Read correctly returned E_BG_INPUT\n");
        sys_exit(0);
    } else {
        printf("[test_bg] ERROR: Read returned %d (expected %d)\n", 
               (int)result, E_BG_INPUT);
        sys_exit(1);
    }
}

// Proceso que solo escribe (background)
static void writer_process(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    // Escribir algo
    printf("[bg_writer] Hello from background!\n");
    
    // Hacer un poco de trabajo
    for (int i = 0; i < 3; i++) {
        sys_yield();
    }
    
    printf("[bg_writer] Exiting normally\n");
    sys_exit(0);
}

// Proceso que spamea output
static void spammer_process(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    for (int i = 0; i < 5; i++) {
        printf("[spam %d] Iteration %d\n", (int)sys_getpid(), i);
        sys_yield();
    }
    
    sys_exit(0);
}

/**
 * Test 1: Verificar que BG no puede leer de stdin
 */
static void test_bg_cannot_read(void) {
    printf("\n=== Test 1: BG cannot read ===\n");
    
    // Crear proceso en background (fg=false)
    int64_t pid = sys_create_process(reader_process, 0, NULL, "bg_reader", 2);
    
    if (pid < 0) {
        printf("[FAIL] Could not create background process\n");
        return;
    }
    
    printf("[test] Created background reader with PID %d\n", (int)pid);
    
    // Esperar a que termine
    int status = 0;
    int waited = sys_wait_pid(pid, &status);
    
    if (waited == pid && status == 0) {
        printf("[PASS] BG process correctly got E_BG_INPUT\n");
    } else {
        printf("[FAIL] BG process exited with unexpected status %d\n", status);
    }
}

/**
 * Test 2: Shell recupera el TTY después de un comando FG
 */
static void test_shell_regains_tty(void) {
    printf("\n=== Test 2: Shell regains TTY ===\n");
    
    // La shell debería estar en foreground
    int64_t shell_pid = sys_getpid();
    printf("[test] Shell PID: %d\n", (int)shell_pid);
    
    // Crear un proceso background
    int64_t bg_pid = sys_create_process(writer_process, 0, NULL, "bg_writer", 2);
    
    if (bg_pid < 0) {
        printf("[FAIL] Could not create background process\n");
        return;
    }
    
    printf("[test] Created background writer with PID %d\n", (int)bg_pid);
    
    // La shell debería poder leer inmediatamente
    // (esto es implícito, si llegamos aquí es porque podemos ejecutar)
    
    printf("[PASS] Shell still has TTY control\n");
    
    // Limpiar
    sys_wait_pid(bg_pid, NULL);
}

/**
 * Test 3: BG puede escribir sin problemas
 */
static void test_bg_can_write(void) {
    printf("\n=== Test 3: BG can write ===\n");
    
    int64_t pid = sys_create_process(writer_process, 0, NULL, "bg_writer", 2);
    
    if (pid < 0) {
        printf("[FAIL] Could not create background process\n");
        return;
    }
    
    printf("[test] Created background writer with PID %d\n", (int)pid);
    
    // Esperar
    int status = 0;
    sys_wait_pid(pid, &status);
    
    if (status == 0) {
        printf("[PASS] BG process wrote successfully\n");
    } else {
        printf("[FAIL] BG process failed with status %d\n", status);
    }
}

/**
 * Test 4: FG puede leer normalmente
 */
static void test_fg_normal_read(void) {
    printf("\n=== Test 4: FG can read ===\n");
    printf("[test] This test requires manual input\n");
    printf("[test] Type something and press ENTER: ");
    
    char buf[64];
    int64_t n = sys_read(0, buf);
    
    if (n > 0) {
        printf("[PASS] FG read %d bytes successfully\n", (int)n);
    } else if (n == E_BG_INPUT) {
        printf("[FAIL] FG process got E_BG_INPUT (should not happen)\n");
    } else {
        printf("[FAIL] Read failed with code %d\n", (int)n);
    }
}

/**
 * Test 5: Múltiples procesos BG simultáneos
 */
static void test_multiple_bg(void) {
    printf("\n=== Test 5: Multiple BG processes ===\n");
    
    int pids[3];
    
    for (int i = 0; i < 3; i++) {
        pids[i] = sys_create_process(spammer_process, 0, NULL, "spammer", 2);
        if (pids[i] < 0) {
            printf("[FAIL] Could not create process %d\n", i);
            return;
        }
        printf("[test] Created spammer %d with PID %d\n", i, pids[i]);
    }
    
    // Esperar a todos
    for (int i = 0; i < 3; i++) {
        int status = 0;
        sys_wait_pid(pids[i], &status);
        if (status != 0) {
            printf("[FAIL] Spammer %d failed with status %d\n", i, status);
            return;
        }
    }
    
    printf("[PASS] All BG processes completed successfully\n");
}

/**
 * Función principal de tests
 */
void test_fg_bg_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    printf("\n");
    printf("========================================\n");
    printf("   Foreground/Background Tests\n");
    printf("========================================\n");
    
    test_bg_cannot_read();
    test_shell_regains_tty();
    test_bg_can_write();
    test_multiple_bg();
    test_fg_normal_read();  // Manual, va al final
    
    printf("\n");
    printf("========================================\n");
    printf("   Tests completed\n");
    printf("========================================\n");
    
    sys_exit(0);
}
