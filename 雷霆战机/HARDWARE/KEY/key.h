#ifndef __KEY_H
#define __KEY_H
#include "sys.h"

//////////////////////////////////////////////////////////////////////////////////
// KEY0  -> PE2  -> Right (active LOW)
// KEY1  -> PE3  -> Down  (active LOW)
// KEY2  -> PE4  -> Left  (active LOW)
// KEY_UP-> PA0  -> Up    (active HIGH)
//////////////////////////////////////////////////////////////////////////////////

#define KEY_UP    PAin(0)
#define KEY0      PEin(2)
#define KEY1      PEin(3)
#define KEY2      PEin(4)

#define KEY_UP_PRES  1
#define KEY0_PRES    2
#define KEY1_PRES    3
#define KEY2_PRES    4

void KEY_Init(void);
u8   KEY_Scan(u8 mode);
#endif
