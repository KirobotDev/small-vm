#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "vm.h"
#include "font.h"

void vm_init(VM *vm, bool headless) {
    memset(vm->regs, 0, sizeof(vm->regs));
    memset(vm->memory, 0, sizeof(vm->memory));
    vm->pc = 0;
    vm->sp = MEM_SIZE - 2; 
    vm->z_flag = false;
    vm->running = false;
    vm->is_headless = headless;
    
    vm->cursor_x = 0;
    vm->cursor_y = 0;
    vm->smooth_x = 0;
    vm->smooth_y = 0;
    vm->kbd_head = 0;
    vm->kbd_tail = 0;
    memset(vm->kbd_buffer, 0, sizeof(vm->kbd_buffer));

    if (headless) {
        vm->window = NULL;
        vm->renderer = NULL;
        vm->texture = NULL;
        memset(vm->screen_buffer, 0, sizeof(vm->screen_buffer));
        return;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Warning: SDL could not initialize! Running in Headless Mode. SDL_Error: %s\n", SDL_GetError());
        vm->window = NULL;
        vm->renderer = NULL;
        vm->texture = NULL;
    } else {
        vm->window = SDL_CreateWindow("Mini VM - Monitor", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 480, SDL_WINDOW_SHOWN);
        if (!vm->window) {
            fprintf(stderr, "Warning: Window could not be created! Headless Mode. SDL_Error: %s\n", SDL_GetError());
            vm->renderer = NULL;
            vm->texture = NULL;
        } else {
            vm->renderer = SDL_CreateRenderer(vm->window, -1, SDL_RENDERER_ACCELERATED);
            vm->texture = SDL_CreateTexture(vm->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 640, 240);
            SDL_StartTextInput(); 
        }
    }
    memset(vm->screen_buffer, 0, sizeof(vm->screen_buffer));
}

void vm_load_program(VM *vm, const uint8_t *program, size_t size) {
    if (size > MEM_SIZE) {
        fprintf(stderr, "Error: Program size (%zu) exceeds memory size (%d)\n", size, MEM_SIZE);
        return;
    }
    memcpy(vm->memory, program, size);
}

static uint16_t mem_read16(VM *vm, uint16_t addr) {
    return vm->memory[addr] | (vm->memory[addr + 1] << 8);
}

static void disk_handler(VM *vm, uint8_t cmd) {
    FILE *disk = fopen("disk.img", "rb+"); 
    if (!disk) {
        disk = fopen("disk.img", "wb+"); 
        if (!disk) return;
        uint32_t magic = 0xDEADBEEF;
        uint16_t count = 0;
        fwrite(&magic, 4, 1, disk);
        fwrite(&count, 2, 1, disk);
        fflush(disk);
    }

    uint16_t ptr_name = vm->memory[DISK_ARG1] | (vm->memory[DISK_ARG1+1] << 8);
    uint16_t ptr_data = vm->memory[DISK_ARG2] | (vm->memory[DISK_ARG2+1] << 8);
    uint16_t size_arg = vm->memory[DISK_ARG3] | (vm->memory[DISK_ARG3+1] << 8);

    if (cmd == 1) { 
        uint16_t count;
        fseek(disk, 4, SEEK_SET); 
        fread(&count, 2, 1, disk);
        
        uint16_t write_addr = ptr_data; 
        for (int i=0; i<count; i++) {
            char name[16];
            fseek(disk, 6 + (i * 24), SEEK_SET); 
            fread(name, 16, 1, disk);
            for(int j=0; j<16; j++) {
                 if (write_addr + j < MEM_SIZE) vm->memory[write_addr + j] = name[j];
            }
            if (write_addr + 16 < MEM_SIZE) vm->memory[write_addr + 16] = '\n'; 
            write_addr += 17;
        }
        vm->memory[write_addr] = 0; 
    } 
    else if (cmd == 2) { 
        char target_name[16];
        memset(target_name, 0, 16);
        for(int i=0; i<15; i++) target_name[i] = vm->memory[ptr_name+i];
        
        uint16_t count;
        fseek(disk, 4, SEEK_SET);
        fread(&count, 2, 1, disk);
        
        for (int i=0; i<count; i++) {
            char name[16];
            uint32_t offset, size;
            fseek(disk, 6 + (i * 24), SEEK_SET);
            fread(name, 16, 1, disk);
            fread(&offset, 4, 1, disk);
            fread(&size, 4, 1, disk);
            
            if (strncmp(name, target_name, 16) == 0) {
                fseek(disk, offset, SEEK_SET);
                for(uint32_t b=0; b<size; b++) {
                    if (ptr_data + b < MEM_SIZE) {
                         uint8_t byte;
                         fread(&byte, 1, 1, disk);
                         vm->memory[ptr_data + b] = byte;
                    }
                }
                vm->memory[DISK_ARG3] = size & 0xFF;
                vm->memory[DISK_ARG3+1] = (size >> 8) & 0xFF;
                break;
            }
        }
    }
    else if (cmd == 3) { 
        char target_name[16];
        memset(target_name, 0, 16);
        for(int i=0; i<15; i++) target_name[i] = vm->memory[ptr_name+i];

        uint16_t count;
        fseek(disk, 4, SEEK_SET);
        fread(&count, 2, 1, disk);
        
        fseek(disk, 0, SEEK_END);
        long data_offset = ftell(disk);
        for(int i=0; i<size_arg; i++) {
            fwrite(&vm->memory[ptr_data + i], 1, 1, disk);
        }
        
        fseek(disk, 6 + (count * 24), SEEK_SET);
        fwrite(target_name, 16, 1, disk);
        fwrite(&data_offset, 4, 1, disk);
        uint32_t s32 = size_arg;
        fwrite(&s32, 4, 1, disk);
        
        count++;
        fseek(disk, 4, SEEK_SET);
        fwrite(&count, 2, 1, disk);
    }

    fclose(disk);
}

static void gpu_update(VM *vm) {
    if (!vm->window) return;

    memset(vm->screen_buffer, 0, sizeof(vm->screen_buffer));

    for (int row = 0; row < VRAM_ROWS; row++) {
        for (int col = 0; col < VRAM_COLS; col++) {
            uint16_t addr = VRAM_START + (row * VRAM_COLS) + col;
            uint8_t char_code = vm->memory[addr];
            
            int char_index = char_code - 32;
            if (char_index < 0 || char_index >= 96) char_index = 0;
            
            const uint8_t *glyph = font8x8_basic[char_index];
            
            int screen_x = col * 8;
            int screen_y = row * 8;
            
            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 8; c++) {
                    if (glyph[r] & (1 << (7-c))) {
                         vm->screen_buffer[(screen_y + r) * 640 + (screen_x + c)] = 0xFFFFFFFF;
                    }
                }
            }
        }
    }
    
    float target_x = vm->cursor_x * 8.0f;
    float target_y = vm->cursor_y * 8.0f;
    
    vm->smooth_x += (target_x - vm->smooth_x) * 0.2f;
    vm->smooth_y += (target_y - vm->smooth_y) * 0.2f;

    int cx = (int)vm->smooth_x;
    int cy = (int)vm->smooth_y;
    
    if (cy < (VRAM_ROWS * 8) && cx < (VRAM_COLS * 8)) {
        for(int r=6; r<8; r++) { 
            for(int c=0; c<8; c++) {
                 int px = cx + c;
                 int py = cy + r;
                 if (px < 640 && py < 240)
                    vm->screen_buffer[py * 640 + px] ^= 0xFFFFFFFF; 
            }
        }
    }

    int pitch;
    void *pixels;
    if (SDL_LockTexture(vm->texture, NULL, &pixels, &pitch) == 0) {
        memcpy(pixels, vm->screen_buffer, 640 * 240 * 4);
        SDL_UnlockTexture(vm->texture);
    }
    SDL_RenderCopy(vm->renderer, vm->texture, NULL, NULL);
    SDL_RenderPresent(vm->renderer);
}

static void console_putc(VM *vm, char c) {
    if (c == '\n') {
        vm->cursor_x = 0;
        vm->cursor_y++;
    } else if (c == '\r') {
        vm->cursor_x = 0;
    } else if (c == 8) { 
        if (vm->cursor_x > 0) {
            vm->cursor_x--;
        } else if (vm->cursor_y > 0) {
            vm->cursor_y--;
            vm->cursor_x = VRAM_COLS - 1;
        }
    } else {
        uint16_t addr = VRAM_START + (vm->cursor_y * VRAM_COLS) + vm->cursor_x;
        if (addr < VRAM_START + VRAM_SIZE) {
            vm->memory[addr] = (uint8_t)c;
        }
        vm->cursor_x++;
        if (vm->cursor_x >= VRAM_COLS) {
            vm->cursor_x = 0;
            vm->cursor_y++;
        }
    }
    
    if (vm->cursor_y >= VRAM_ROWS) {
        int rows_to_move = VRAM_ROWS - 1;
        memmove(&vm->memory[VRAM_START], &vm->memory[VRAM_START + VRAM_COLS], rows_to_move * VRAM_COLS);
        memset(&vm->memory[VRAM_START + (rows_to_move * VRAM_COLS)], 0, VRAM_COLS);
        vm->cursor_y = VRAM_ROWS - 1;
    }
}

static void mem_write8(VM *vm, uint16_t addr, uint8_t val) {
    vm->memory[addr] = val; 
    
    if (addr == DISK_CMD) {
        disk_handler(vm, val);
    }
}

static void mem_write16(VM *vm, uint16_t addr, uint16_t val) {
    mem_write8(vm, addr, val & 0xFF);
    mem_write8(vm, addr + 1, (val >> 8) & 0xFF);
}

void vm_cpu_step(VM *vm) {

    uint8_t opcode = vm->memory[vm->pc];
    vm->pc++;

    switch (opcode) {
        case OP_LOAD: { 
            uint8_t reg = vm->memory[vm->pc++];
            uint16_t val = mem_read16(vm, vm->pc);
            vm->pc += 2;
            if (reg < REG_COUNT) vm->regs[reg] = val;
            break;
        }
        case OP_ADD: { 
            uint8_t r1 = vm->memory[vm->pc++];
            uint8_t r2 = vm->memory[vm->pc++];
            if (r1 < REG_COUNT && r2 < REG_COUNT) {
                vm->regs[r1] += vm->regs[r2];
                vm->z_flag = (vm->regs[r1] == 0);
            }
            break;
        }
        case OP_SUB: { 
            uint8_t r1 = vm->memory[vm->pc++];
            uint8_t r2 = vm->memory[vm->pc++];
            if (r1 < REG_COUNT && r2 < REG_COUNT) {
                vm->regs[r1] -= vm->regs[r2];
                vm->z_flag = (vm->regs[r1] == 0);
            }
            break;
        }
        case OP_LOADM: { 
            uint8_t reg = vm->memory[vm->pc++];
            uint16_t addr = mem_read16(vm, vm->pc);
            vm->pc += 2;
            if (reg < REG_COUNT) {
                vm->regs[reg] = mem_read16(vm, addr); 
            }
            break;
        }
        case OP_STORE: { 
            uint8_t reg = vm->memory[vm->pc++];
            uint16_t addr = mem_read16(vm, vm->pc);
            vm->pc += 2;
            if (reg < REG_COUNT) {
                mem_write16(vm, addr, vm->regs[reg]); 
            }
            break;
        }
        case OP_JMP: { 
            uint16_t addr = mem_read16(vm, vm->pc);
            vm->pc = addr;
            break;
        }
        case OP_JZ: { 
            uint16_t addr = mem_read16(vm, vm->pc);
            if (vm->z_flag) {
                vm->pc = addr;
            } else {
                vm->pc += 2; 
            }
            break;
        }
        case OP_PRINT: { 
            uint8_t reg = vm->memory[vm->pc++];
            if (reg < REG_COUNT) {
                printf("OUT: %d\n", vm->regs[reg]);
            }
            break;
        }
        case OP_CMP: {
            uint8_t r1 = vm->memory[vm->pc++];
            uint8_t r2 = vm->memory[vm->pc++];
            if (r1 < REG_COUNT && r2 < REG_COUNT) {
                vm->z_flag = (vm->regs[r1] == vm->regs[r2]);
            }
            break;
        }
        case OP_PUSH: { 
            uint8_t reg = vm->memory[vm->pc++];
            if (reg < REG_COUNT) {
                mem_write16(vm, vm->sp, vm->regs[reg]);
                vm->sp -= 2;
            }
            break;
        }
        case OP_POP: { 
            uint8_t reg = vm->memory[vm->pc++];
            if (reg < REG_COUNT) {
                vm->sp += 2;
                vm->regs[reg] = mem_read16(vm, vm->sp);
            }
            break;
        }
        case OP_CALL: { 
            uint16_t addr = mem_read16(vm, vm->pc);
            vm->pc += 2; 
            
            mem_write16(vm, vm->sp, vm->pc);
            vm->sp -= 2;
            
            vm->pc = addr; 
            break;
        }
        case OP_RET: { 
            vm->sp += 2;
            uint16_t ret_addr = mem_read16(vm, vm->sp);
            vm->pc = ret_addr;
            break;
        }
        case OP_IN: { 
            uint8_t reg = vm->memory[vm->pc++];
            if (reg < REG_COUNT) {
                 if (vm->is_headless) {
                     int c = getchar();
                     if (c == EOF) vm->running = false;
                     vm->regs[reg] = (uint16_t)c;
                } else {
                     if (vm->kbd_head != vm->kbd_tail) {
                        vm->regs[reg] = vm->kbd_buffer[vm->kbd_tail];
                        vm->kbd_tail = (vm->kbd_tail + 1) % 64;
                     } else {
                         vm->pc -= 2;
                     }
                }
            }
            break;
        }
        case OP_OUT: { 
            uint8_t reg = vm->memory[vm->pc++];
            if (reg < REG_COUNT) {
                char c = (char)vm->regs[reg];
                if (!vm->is_headless) {
                    console_putc(vm, c);
                } else {
                    putchar(c);
                    fflush(stdout); 
                }
            }
            break;
        }
        case OP_LOADR: { 
            uint8_t r_dest = vm->memory[vm->pc++];
            uint8_t r_addr = vm->memory[vm->pc++];
            if (r_dest < REG_COUNT && r_addr < REG_COUNT) {
                uint16_t addr = vm->regs[r_addr];
                vm->regs[r_dest] = mem_read16(vm, addr);
            }
            break;
        }
        case OP_STORER: { 
            uint8_t r_val = vm->memory[vm->pc++];
            uint8_t r_addr = vm->memory[vm->pc++];
            if (r_val < REG_COUNT && r_addr < REG_COUNT) {
                uint16_t addr = vm->regs[r_addr];
                mem_write16(vm, addr, vm->regs[r_val]);
            }
            break;
        }
        case OP_HALT: {
            vm->running = false;
            printf("VM Halted.\n");
            break;
        }
        default:
            printf("Unknown Opcode: 0x%02X at %d\n", opcode, vm->pc - 1);
            vm->running = false;
            break;
    }

    if (!vm->is_headless) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                vm->running = false;
            }
            else if (e.type == SDL_KEYDOWN) {
                 SDL_Keycode sym = e.key.keysym.sym;
                 uint16_t mod = e.key.keysym.mod;
                 
                 if ((mod & KMOD_CTRL) && sym >= SDLK_a && sym <= SDLK_z) {
                     uint8_t ctrl_code = (sym - SDLK_a) + 1;
                     vm->kbd_buffer[vm->kbd_head] = ctrl_code;
                     vm->kbd_head = (vm->kbd_head + 1) % 64;
                 }
                 else if (sym == SDLK_ESCAPE) {
                     vm->running = false;
                 } else if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
                     vm->kbd_buffer[vm->kbd_head] = 10; 
                     vm->kbd_head = (vm->kbd_head + 1) % 64;
                 } else if (sym == SDLK_BACKSPACE) {
                     vm->kbd_buffer[vm->kbd_head] = 8;
                     vm->kbd_head = (vm->kbd_head + 1) % 64;
                 }
            }
            else if (e.type == SDL_TEXTINPUT) {
                  if (SDL_GetModState() & KMOD_CTRL) return;

                 for (int i=0; e.text.text[i] != '\0'; i++) {
                     unsigned char c = (unsigned char)e.text.text[i];
                     if (c >= 32 && c <= 126) {
                        vm->kbd_buffer[vm->kbd_head] = c;
                        vm->kbd_head = (vm->kbd_head + 1) % 64;
                     } else {
                     }
                 }
            }
        }
        gpu_update(vm);
    }
}

void vm_run(VM *vm) {
    vm->running = true;
    while (vm->running) {
        vm_cpu_step(vm);
        SDL_Delay(1); 
    }
    
    SDL_DestroyTexture(vm->texture);
    SDL_DestroyRenderer(vm->renderer);
    SDL_DestroyWindow(vm->window);
    SDL_Quit();
}
