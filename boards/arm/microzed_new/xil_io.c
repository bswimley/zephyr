#include "xil_io.h"

uint32_t Xil_In32(uint32_t addr) {
    return *(volatile uint32_t *)addr;
}

void Xil_Out32(uint32_t addr, uint32_t value) {
    *(volatile uint32_t *)addr = value;
}
