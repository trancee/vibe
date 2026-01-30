#include "c64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    C64* sys = malloc(sizeof(C64));
    if (!sys) {
        fprintf(stderr, "Failed to allocate C64 system\n");
        return 1;
    }

    c64_init(sys);
    
    printf("C64 Emulator initialized\n");
    printf("ROMs loaded: Kernal=%s, BASIC=%s, Char=%s\n",
           sys->mem.kernal_rom ? "OK" : "FAIL",
           sys->mem.basic_rom ? "OK" : "FAIL", 
           sys->mem.char_rom ? "OK" : "FAIL");
    
    // Run for a few cycles to test basic functionality
    for (int i = 0; i < 100000; i++) {
        c64_step(sys);
    }
    
    printf("Emulation completed. Final PC: $%04X\n", sys->cpu.pc);
    
    c64_cleanup(sys);
    free(sys);
    return 0;
}