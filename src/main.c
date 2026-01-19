#include <stdio.h>
#include <stdlib.h>
#include "vm.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <program.bin> [--headless]\n", argv[0]);
        return 1;
    }

    bool headless = false;
    char *filename = argv[1];
    
    if (argc > 2 && strcmp(argv[2], "--headless") == 0) {
        headless = true;
    }
    
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Failed to open file");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(f);
        return 1;
    }

    if (fread(buffer, 1, size, f) != (size_t)size) {
        fprintf(stderr, "Failed to read file\n");
        free(buffer);
        fclose(f);
        return 1;
    }
    fclose(f);

    VM vm;
    vm_init(&vm, headless);
    vm_load_program(&vm, buffer, size);
    free(buffer);

    printf("Starting Mini VM (Headless: %s)...\n", headless ? "Yes" : "No");
    vm_run(&vm);

    return 0;
}
