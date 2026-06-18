#ifndef __SRAM_H
#define __SRAM_H
#include "sys.h"

#define SRAM_BASE  ((u16*)0x68000000)
#define SRAM_SIZE  (1024 * 1024)  // 1MB

void SRAM_Init(void);
#endif
