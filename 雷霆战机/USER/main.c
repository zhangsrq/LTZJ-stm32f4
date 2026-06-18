#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "lcd.h"
#include "key.h"
#include "sram.h"
#include "game.h"

//////////////////////////////////////////////////////////////////////////////////
// LeiTingZhanJi - Thunder Fighter
// STM32F407 Explorer Board, NT35510 LCD 480x800
// Controls: KEY0=Right, KEY1=Down, KEY2=Left, KEY_UP=Up
// Start game: press KEY_UP on title screen
//////////////////////////////////////////////////////////////////////////////////

int main(void)
{
    u8 key;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    delay_init(168);
    uart_init(115200);
    LED_Init();
    LCD_Init();
    KEY_Init();
    SRAM_Init();   // external 1MB SRAM for double buffer

    // Initialize and show start page
    Game_Init();
    Game_DrawStartPage();

    while (1)
    {
        key = KEY_Scan(1);  // Continuous mode for held-key movement

        switch (Game_GetState())
        {
            case 0: // STATE_START
                if (key == KEY_UP_PRES) {
                    Game_Start();
                }
                break;

            case 1: // STATE_PLAYING
                Game_Update(key);
                Game_Render();
                break;

            case 2: // STATE_GAMEOVER
                if (key == KEY_UP_PRES) {
                    Game_Start();
                }
                break;

            case 3: // STATE_WIN
                if (key == KEY_UP_PRES) {
                    Game_Start();
                }
                break;
        }

       // delay_ms(8);  // ~30 FPS (double-buffer flush takes ~15ms)
        
    }
}
