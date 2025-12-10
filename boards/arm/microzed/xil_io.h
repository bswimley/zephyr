#ifndef XIL_IO_H
#define XIL_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t Xil_In32(uint32_t addr);
void Xil_Out32(uint32_t addr, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif // XIL_IO_H
