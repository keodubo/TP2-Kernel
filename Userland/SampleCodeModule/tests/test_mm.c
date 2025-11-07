#include "syscall.h"
#include "test_util.h"
#include <stdio.h>
#include "../include/sys_calls.h"
#include "../include/userlib.h"
#include "../include/mm_stats.h"

#define MAX_BLOCKS 128

typedef struct MM_rq {
  void *address;
  uint32_t size;
} mm_rq;

uint64_t test_mm(uint64_t argc, char *argv[]) {

  mm_rq mm_rqs[MAX_BLOCKS];
  uint8_t rq;
  uint32_t total;
  uint64_t max_memory;

  printf("Iniciando test_mm con %s bytes\n", argv[0]);

  if (argc != 1)
    return -1;

  if ((max_memory = satoi(argv[0])) <= 0)
    return -1;

  while (1) {
    printf("Ciclo de test_mm iniciado\n");
    rq = 0;
    total = 0;

    // Request as many blocks as we can
    while (rq < MAX_BLOCKS && total < max_memory) {
      // Limitar tamaño maximo para evitar bloques gigantes
      uint32_t max_block_size = (max_memory - total > 1024) ? 1024 : (max_memory - total);
      if (max_block_size < 8) break; // Tamaño minimo
      
      mm_rqs[rq].size = GetUniform(max_block_size - 8) + 8; // Entre 8 y max_block_size bytes
      printf("Intentando asignar bloque %d de %d bytes\n", rq, mm_rqs[rq].size);
      mm_rqs[rq].address = sys_malloc(mm_rqs[rq].size);

      if (mm_rqs[rq].address) {
        total += mm_rqs[rq].size;
        rq++;
        printf("Bloque %d asignado exitosamente\n", rq-1);
        
        // Imprimir información del heap como en comando mem
        mm_stats_t stats;
        if (sys_mm_get_stats(&stats) >= 0) {
          printf("heap: %d bytes\n", (int)stats.heap_total);
          printf("used: %d bytes\n", (int)stats.used_bytes);
          printf("free: %d bytes\n", (int)stats.free_bytes);
        }
      } else {
        printf("Error: no se pudo asignar bloque %d\n", rq);
        break;
      }
    }
    printf("Total asignados: %d bloques, %d bytes\n", rq, total);

    // Set
    uint32_t i;
    for (i = 0; i < rq; i++)
      if (mm_rqs[i].address)
        memset(mm_rqs[i].address, i, mm_rqs[i].size);

    // Verificar integridad
    for (i = 0; i < rq; i++)
      if (mm_rqs[i].address)
        if (!memcheck(mm_rqs[i].address, i, mm_rqs[i].size)) {
          printf("test_mm ERROR\n");
          return -1;
        }

    // Liberar memoria
    for (i = 0; i < rq; i++)
      if (mm_rqs[i].address)
        sys_free(mm_rqs[i].address);
    printf("Ciclo completado\n");
  }
  
  return 0;
}