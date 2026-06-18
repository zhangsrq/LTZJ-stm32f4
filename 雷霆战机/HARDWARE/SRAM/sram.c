#include "sram.h"
#include "delay.h"

//////////////////////////////////////////////////////////////////////////////////
// External SRAM (IS62WV51216, 1MB) on FSMC Bank1 NE3, 0x68000000
// Only address lines and NE3 are new; data bus shared with LCD
//////////////////////////////////////////////////////////////////////////////////

void SRAM_Init(void)
{
    FSMC_NORSRAMInitTypeDef       FSMC_NORSRAMInitStructure;
    FSMC_NORSRAMTimingInitTypeDef FSMC_NORSRAMTimingInitStructure;
    GPIO_InitTypeDef              GPIO_InitStructure;

    // Clocks already enabled by LCD_Init:
    // RCC_AHB1: GPIOB|GPIOD|GPIOE|GPIOF|GPIOG
    // RCC_AHB3: FSMC

    // --- GPIO: configure SRAM-specific FSMC pins ---
    // These are NOT configured by LCD_Init, only by us

    // NE3 - PG10
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOG, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOG, GPIO_PinSource10, GPIO_AF_FSMC);

    // FSMC_A0-A5 on PF0-PF5
    GPIO_InitStructure.GPIO_Pin   = 0x003F;  // PF0..PF5
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOF, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource0,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource1,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource2,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource3,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource4,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource5,  GPIO_AF_FSMC);

    // FSMC_A7-A9 on PF13-PF15 (PF12 = A6 already config'd by LCD_Init)
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOF, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource13, GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource14, GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOF, GPIO_PinSource15, GPIO_AF_FSMC);

    // FSMC_A10-A15 on PG0-PG5
    GPIO_InitStructure.GPIO_Pin   = 0x003F;  // PG0..PG5
    GPIO_Init(GPIOG, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOG, GPIO_PinSource0,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOG, GPIO_PinSource1,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOG, GPIO_PinSource2,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOG, GPIO_PinSource3,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOG, GPIO_PinSource4,  GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOG, GPIO_PinSource5,  GPIO_AF_FSMC);

    // FSMC_A16-A18 on PD11-PD13
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource11, GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource12, GPIO_AF_FSMC);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource13, GPIO_AF_FSMC);

    // --- FSMC timing for SRAM (fast, 0 wait states) ---
    FSMC_NORSRAMTimingInitStructure.FSMC_AddressSetupTime       = 2;
    FSMC_NORSRAMTimingInitStructure.FSMC_AddressHoldTime        = 0;
    FSMC_NORSRAMTimingInitStructure.FSMC_DataSetupTime          = 2;
    FSMC_NORSRAMTimingInitStructure.FSMC_BusTurnAroundDuration  = 0;
    FSMC_NORSRAMTimingInitStructure.FSMC_CLKDivision            = 0;
    FSMC_NORSRAMTimingInitStructure.FSMC_DataLatency            = 0;
    FSMC_NORSRAMTimingInitStructure.FSMC_AccessMode             = FSMC_AccessMode_A;

    FSMC_NORSRAMInitStructure.FSMC_Bank             = FSMC_Bank1_NORSRAM3;   // NE3
    FSMC_NORSRAMInitStructure.FSMC_DataAddressMux   = FSMC_DataAddressMux_Disable;
    FSMC_NORSRAMInitStructure.FSMC_MemoryType       = FSMC_MemoryType_SRAM;
    FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth  = FSMC_MemoryDataWidth_16b;
    FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode  = FSMC_BurstAccessMode_Disable;
    FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;
    FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
    FSMC_NORSRAMInitStructure.FSMC_WrapMode         = FSMC_WrapMode_Disable;
    FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
    FSMC_NORSRAMInitStructure.FSMC_WriteOperation   = FSMC_WriteOperation_Enable;
    FSMC_NORSRAMInitStructure.FSMC_WaitSignal       = FSMC_WaitSignal_Disable;
    FSMC_NORSRAMInitStructure.FSMC_ExtendedMode     = FSMC_ExtendedMode_Disable;
    FSMC_NORSRAMInitStructure.FSMC_WriteBurst       = FSMC_WriteBurst_Disable;
    FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &FSMC_NORSRAMTimingInitStructure;
    FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct     = &FSMC_NORSRAMTimingInitStructure;

    FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);
    FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM3, ENABLE);
}
