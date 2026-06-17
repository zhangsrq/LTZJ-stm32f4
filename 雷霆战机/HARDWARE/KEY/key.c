#include "key.h"
#include "delay.h"

//////////////////////////////////////////////////////////////////////////////////
// Key GPIO init:
//   KEY_UP (PA0): pull-down input, press = HIGH
//   KEY0  (PE2): pull-up input,  press = LOW
//   KEY1  (PE3): pull-up input,  press = LOW
//   KEY2  (PE4): pull-up input,  press = LOW
//////////////////////////////////////////////////////////////////////////////////

void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOE, ENABLE);

    // KEY_UP (PA0): pull-down
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // KEY0 (PE2), KEY1 (PE3), KEY2 (PE4): pull-up
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOE, &GPIO_InitStructure);
}

//////////////////////////////////////////////////////////////////////////////////
// Scan keys with debounce
// mode=0: single-shot (returns key only on first press, not held)
// mode=1: continuous  (returns key while held, supports continuous movement)
//////////////////////////////////////////////////////////////////////////////////
u8 KEY_Scan(u8 mode)
{
    static u8 key_up_flag = 1;  // 1 = released, ready for new press

    if (mode) key_up_flag = 1;  // continuous mode: always retrigger

    if (key_up_flag &&
        (KEY_UP == 1 || KEY0 == 0 || KEY1 == 0 || KEY2 == 0))
    {
        delay_ms(10);  // debounce

        key_up_flag = 0;

        if (KEY_UP == 1)  return KEY_UP_PRES;
        if (KEY0 == 0)    return KEY0_PRES;
        if (KEY1 == 0)    return KEY1_PRES;
        if (KEY2 == 0)    return KEY2_PRES;
    }
    else if (KEY_UP == 0 && KEY0 == 1 && KEY1 == 1 && KEY2 == 1)
    {
        key_up_flag = 1;
    }

    return 0;  // no key pressed
}
