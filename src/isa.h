#ifndef ISA_H
#define ISA_H

#include <stdint.h>

typedef enum {
    OP_LOAD   = 0x01, 
    OP_ADD    = 0x02, 
    OP_SUB    = 0x03, 
    OP_LOADM  = 0x04, 
    OP_STORE  = 0x05, 
    OP_JMP    = 0x06, 
    OP_JZ     = 0x07, 
    OP_PRINT  = 0x08, 
    OP_PUSH   = 0x09, 
    OP_POP    = 0x0A, 
    OP_CALL   = 0x0B, 
    OP_RET    = 0x0C, 
    OP_IN     = 0x0D, 
    OP_OUT    = 0x0E, 
    OP_CMP    = 0x0F, 
    OP_LOADR  = 0x10, 
    OP_STORER = 0x11, 
    OP_HALT   = 0xFF
} Opcode;

#endif
