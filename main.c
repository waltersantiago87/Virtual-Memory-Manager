#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#define PAGE_SIZE 256
#define NUM_PAGES 256
#define NUM_FRAMES 128
#define TLB_SIZE 16
#define MAX_ADDRESS 65535

typedef enum {
    FIFO,
    LRU
} Algorithm;

typedef struct {
    int frame;
    int valid;
} PageEntry;

typedef struct {
    int page;
    uint64_t loaded_at;
    uint64_t last_used;
} FrameEntry;

typedef struct {
    int page;
    int frame;
    int valid;
    uint64_t loaded_at;
    uint64_t last_used;
} TLBEntry;

typedef struct {
    int8_t physical_memory[NUM_FRAMES][PAGE_SIZE];
    PageEntry page_table[NUM_PAGES];
    FrameEntry frames[NUM_FRAMES];
    TLBEntry tlb[TLB_SIZE];

    uint64_t clock;
    Algorithm page_algorithm;
    Algorithm tlb_algorithm;
} VirtualMemory;

int parse_algorithm(const char *name, Algorithm *algorithm) {
    if (strcmp(name, "fifo") == 0) {
        *algorithm = FIFO;
        return 1;
    }

    if (strcmp(name, "lru") == 0) {
        *algorithm = LRU;
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    VirtualMemory vm = {0};

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <address_file> <fifo|lru> <fifo|lru>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if (!parse_algorithm(argv[2], &vm.page_algorithm)) {
        fprintf(stderr, "Error: invalid page replacement algorithm.\n");
        return EXIT_FAILURE;
    }

    if (!parse_algorithm(argv[3], &vm.tlb_algorithm)) {
        fprintf(stderr, "Error: invalid TLB replacement algorithm.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
