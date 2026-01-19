#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "isa.h"

#define MEM_SIZE 65536 
#define VRAM_START 0xF000
#define VRAM_COLS 80
#define VRAM_ROWS 30
#define VRAM_SIZE (VRAM_COLS * VRAM_ROWS) 

#define DISK_CMD   0xE000 
#define DISK_ARG1  0xE002 
#define DISK_ARG2  0xE004 
#define DISK_ARG3  0xE006
#define REG_COUNT 8 

typedef struct {
    uint16_t regs[REG_COUNT]; 
    uint16_t pc;              
    uint16_t sp;              
    bool z_flag;              
    uint8_t memory[MEM_SIZE]; 
    bool running;             
    bool is_headless;         

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint32_t screen_buffer[640 * 240]; 

    int cursor_x;
    int cursor_y;
    float smooth_x;
    float smooth_y;
    
    uint8_t kbd_buffer[64];
    int kbd_head;
    int kbd_tail;
} VM;

void vm_init(VM *vm, bool headless);
void vm_load_program(VM *vm, const uint8_t *program, size_t size);
void vm_cpu_step(VM *vm);
void vm_run(VM *vm);
void vm_dump_state(VM *vm);

#endif
