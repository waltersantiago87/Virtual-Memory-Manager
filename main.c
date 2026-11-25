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



int select_fifo_frame(const VirtualMemory *vm) {
    int victim = 0;

    for (int i = 1; i < NUM_FRAMES; i++) {
        if (vm->frames[i].loaded_at <
            vm->frames[victim].loaded_at) {
            victim = i;
        }
    }

    return victim;
}


int select_lru_frame(const VirtualMemory *vm) {
    int victim = 0;

    for (int i = 1; i < NUM_FRAMES; i++) {
        if (vm->frames[i].last_used <
            vm->frames[victim].last_used) {
            victim = i;
        }
    }

    return victim;
}

int load_page(VirtualMemory *vm, FILE *backing_store, int page) {
    int frame = find_free_frame(vm);

    if (frame == -1) {
        if (vm->page_algorithm == FIFO) {
            frame = select_fifo_frame(vm);
        } else {
            frame = select_lru_frame(vm);
        }
    }

    if (!read_page(backing_store, page,
                   vm->physical_memory[frame])) {
        return -1;
    }

    int old_page = vm->frames[frame].page;

    if (old_page != -1) {
        vm->page_table[old_page].valid = 0;
        vm->page_table[old_page].frame = -1;
    }

    vm->frames[frame].page = page;
    vm->frames[frame].loaded_at = vm->clock;
    vm->frames[frame].last_used = vm->clock;

    vm->page_table[page].frame = frame;
    vm->page_table[page].valid = 1;

    return frame;
}


void initialize_tlb(VirtualMemory *vm) {
    for (int i = 0; i < TLB_SIZE; i++) {
        vm->tlb[i].page = -1;
        vm->tlb[i].frame = -1;
        vm->tlb[i].valid = 0;
        vm->tlb[i].loaded_at = 0;
        vm->tlb[i].last_used = 0;
    }
}

int find_free_tlb_entry(const VirtualMemory *vm) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (!vm->tlb[i].valid) {
            return i;
        }
    }

    return -1;
}


int search_tlb(const VirtualMemory *vm, int page) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (vm->tlb[i].valid &&
            vm->tlb[i].page == page) {
            return i;
        }
    }

    return -1;
}


int select_fifo_tlb(const VirtualMemory *vm) {
    int victim = 0;

    for (int i = 1; i < TLB_SIZE; i++) {
        if (vm->tlb[i].loaded_at <
            vm->tlb[victim].loaded_at) {
            victim = i;
        }
    }

    return victim;
}


int select_lru_tlb(const VirtualMemory *vm) {
    int victim = 0;

    for (int i = 1; i < TLB_SIZE; i++) {
        if (vm->tlb[i].last_used <
            vm->tlb[victim].last_used) {
            victim = i;
        }
    }

    return victim;
}

int insert_tlb(VirtualMemory *vm, int page, int frame) {
    int index = find_free_tlb_entry(vm);

    if (index == -1) {
        if (vm->tlb_algorithm == FIFO) {
            index = select_fifo_tlb(vm);
        } else {
            index = select_lru_tlb(vm);
        }
    }

    vm->tlb[index].page = page;
    vm->tlb[index].frame = frame;
    vm->tlb[index].valid = 1;
    vm->tlb[index].loaded_at = vm->clock;
    vm->tlb[index].last_used = vm->clock;

    return index;
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
    initialize_tlb(&vm);

    int address;

    while (fscanf(addresses, "%d", &address) == 1) {
        int page = get_page(address);
        int offset = get_offset(address);

        vm.clock++;

        int frame;
        int tlb_index = search_tlb(&vm, page);

        if (tlb_index != -1) {
            frame = vm.tlb[tlb_index].frame;
        } else {
            if (vm.page_table[page].valid) {
                frame = vm.page_table[page].frame;
            } else {
                frame = load_page(&vm, backing_store, page);

                if (frame == -1) {
                    fprintf(stderr, "Error: cannot load page.\n");
                    fclose(backing_store);
                    fclose(addresses);
                    return EXIT_FAILURE;
                }
            }

            tlb_index = insert_tlb(&vm, page, frame);

            if (tlb_index == -1) {
                fprintf(stderr, "Error: TLB is full.\n");
                fclose(backing_store);
                fclose(addresses);
                return EXIT_FAILURE;
            }
        }

        vm.frames[frame].last_used = vm.clock;
        vm.tlb[tlb_index].last_used = vm.clock;

        int physical_address = frame * PAGE_SIZE + offset;
        int value = vm.physical_memory[frame][offset];

        printf("Virtual address: %d TLB: %d Physical address: %d Value: %d\n",
               address, tlb_index, physical_address, value);
    }

    fclose(backing_store);
    fclose(addresses);
    return EXIT_SUCCESS;
}
