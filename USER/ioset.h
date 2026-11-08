#ifndef __IOSET_H
#define	__IOSET_H

#include "stm32f10x.h"
#include <stdio.h>

void GPIO_IRsensInputs_Init(void);
u8 RET_IRSENSE1( void );
u8 RET_IRSENSE2( void );

#endif /* __IOSET_H */
