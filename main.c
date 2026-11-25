#include <stdio.h>
#include <errno.h>
#include <ctype.h>
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


int validate_addresses(FILE *file) {
    char token[64];
    char *end;
    long address;
    int count = 0;

    while (fscanf(file, "%63s", token) == 1) {
        for (size_t i = 0; token[i] != '\0'; i++) {
            if (!isdigit((unsigned char) token[i])) {
                return 0;
            }
        }

        errno = 0;
        address = strtol(token, &end, 10);

        if (errno != 0 || *end != '\0' ||
            address < 0 || address > MAX_ADDRESS) {
            return 0;
        }

        count++;
    }

    if (ferror(file) || count == 0) {
        return 0;
    }

    rewind(file);
    return 1;
}


int get_page(int address) {
    return (address >> 8) & 0xFF;
}

int get_offset(int address) {
    return address & 0xFF;
}


void initialize_memory(VirtualMemory *vm) {
    memset(vm->physical_memory, 0, sizeof(vm->physical_memory));

    vm->clock = 0;

    for (int i = 0; i < NUM_PAGES; i++) {
        vm->page_table[i].frame = -1;
        vm->page_table[i].valid = 0;
    }

    for (int i = 0; i < NUM_FRAMES; i++) {
        vm->frames[i].page = -1;
        vm->frames[i].loaded_at = 0;
        vm->frames[i].last_used = 0;
    }
}

int find_free_frame(const VirtualMemory *vm) {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (vm->frames[i].page == -1) {
            return i;
        }
    }

    return -1;
}


int read_page(FILE *backing_store, int page, int8_t *buffer) {
    long position = (long) page * PAGE_SIZE;

    if (fseek(backing_store, position, SEEK_SET) != 0) {
        return 0;
    }

    if (fread(buffer, 1, PAGE_SIZE, backing_store) != PAGE_SIZE) {
        return 0;
    }

    return 1;
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

    FILE *addresses = fopen(argv[1], "r");

    if (addresses == NULL) {
        fprintf(stderr, "Error: cannot open address file.\n");
        return EXIT_FAILURE;
    }

    if (!validate_addresses(addresses)) {
        fprintf(stderr, "Error: invalid address file format.\n");
        fclose(addresses);
        return EXIT_FAILURE;
    }

    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");

    if (backing_store == NULL) {
        fprintf(stderr, "Error: cannot open backing store.\n");
        fclose(addresses);
        return EXIT_FAILURE;
    }

    initialize_memory(&vm);

    int address;

    while (fscanf(addresses, "%d", &address) == 1) {
        int page = get_page(address);
        int offset = get_offset(address);

        int8_t buffer[PAGE_SIZE];

        if (!read_page(backing_store, page, buffer)) {
            fprintf(stderr, "Error: cannot read page from backing store.\n");
            fclose(backing_store);
            fclose(addresses);
            return EXIT_FAILURE;
        }

        printf("Address: %d Page: %d Offset: %d Value: %d\n",
               address, page, offset, buffer[offset]);
    }

    fclose(backing_store);
    fclose(addresses);
    return EXIT_SUCCESS;
}
