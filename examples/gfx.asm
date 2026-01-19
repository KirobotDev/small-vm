LOAD R0, 255   
LOAD R1, 0      
LOAD R2, 61440  

LOOP:
    
    STORER R0, R2   
    
    
    LOAD R3, 65
    ADD R2, R3      
    
    LOAD R3, 1
    ADD R1, R3
    LOAD R3, 64
    CMP R1, R3
    JZ HALT
    JMP LOOP

HALT:
    HALT
