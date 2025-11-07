#include <stdio.h>
#include <stdint.h>
#include <sys_calls.h>
#include <userlib.h>
#include <mm_stats.h>
#include <spawn_args.h>

static void print_fixed_ratio(uint64_t numerator, uint64_t denominator) {
    if (denominator == 0) {
        printf("N/A");
        return;
    }

    uint64_t scaled = 0;
    if (numerator == 0) {
        scaled = 0;
    } else {
        scaled = (numerator * 100 + denominator / 2) / denominator;
    }

    uint64_t integer = scaled / 100;
    uint64_t fractional = scaled % 100;

    printf("%d", (int)integer);
    printf(".");
    if (fractional < 10) {
        printf("0");
    }
    printf("%d", (int)fractional);
}

static void print_basic_stats(const mm_stats_t *stats) {
    printf("allocator: %s\n", stats->mm_name[0] ? stats->mm_name : "unknown");

    printf("heap:      ");
    printDec(stats->heap_total);
    printf(" bytes\n");

    printf("used:      ");
    printDec(stats->used_bytes);
    printf(" bytes\n");

    printf("free:      ");
    printDec(stats->free_bytes);
    printf(" bytes\n");

    printf("free_blks: ");
    printDec(stats->free_blocks);
    printf("\n");

    printf("largest:   ");
    printDec(stats->largest_free);
    printf(" bytes\n");

    printf("frag:      ");
    if (stats->free_bytes == 0) {
        printf("N/A\n");
    } else {
        uint64_t wasted = 0;
        if (stats->free_bytes > stats->largest_free) {
            wasted = stats->free_bytes - stats->largest_free;
        }
        print_fixed_ratio(wasted, stats->free_bytes);
        printf("\n");
    }

    if (stats->heap_base != 0 || stats->heap_end != 0) {
        printf("heap_base: 0x");
        printHex(stats->heap_base);
        printf("\nheap_end:  0x");
        printHex(stats->heap_end);
        printf("\n");
    }
}

static void print_verbose_simple(const mm_stats_t *stats) {
    printf("\nfreelist (up to %d):\n", MM_MAX_SIMPLE_BLOCKS);
    for (uint32_t i = 0; i < stats->freelist_count && i < MM_MAX_SIMPLE_BLOCKS; i++) {
        const mm_block_info_t *node = &stats->freelist[i];
        printf("  0x");
        printHex(node->addr);
        printf("  ");
        printDec(node->size);
        printf(" bytes\n");
    }
    if (stats->freelist_count == 0) {
        printf("  (no free blocks)\n");
    }
    if (stats->freelist_truncated > 0) {
        printf("  (+ ");
        printDec(stats->freelist_truncated);
        printf(" more)\n");
    }
}

static void print_verbose_buddy(const mm_stats_t *stats) {
    printf("\norder  size       free\n");
    uint8_t limit = stats->max_order;
    if (limit >= MM_MAX_ORDER) {
        limit = MM_MAX_ORDER - 1;
    }
    for (uint8_t i = 0; i <= limit; i++) {
        const mm_order_info_t *info = &stats->orders[i];
        printf("  ");
        printDec(info->order);
        printf("    ");
        printDec(info->block_size);
        printf("    ");
        printDec(info->free_count);
        printf("\n");
    }
}

int mem_command(int argc, char **argv) {
    int verbose = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i] == NULL) {
            continue;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else {
            printf("mem: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    mm_stats_t stats;
    int64_t rc = sys_mm_get_stats(&stats);
    if (rc < 0) {
        printf("mem: mm_get_stats failed (%d)\n", (int)(-rc));
        return 1;
    }

    print_basic_stats(&stats);

    if (verbose) {
        if (stats.has_buddy) {
            print_verbose_buddy(&stats);
        } else {
            print_verbose_simple(&stats);
        }
    }

    return 0;
}

void mem_main(int argc, char **argv) {
    int status = mem_command(argc, argv);
    free_spawn_args(argv, argc);
    sys_exit(status);
}
