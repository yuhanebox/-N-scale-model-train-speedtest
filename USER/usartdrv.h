#ifndef __USARTDRV_H
#define	__USARTDRV_H

#include "stm32f10x.h"
#include <stdio.h>

#define USART1_BAUDRATE 9600

void USART1_Init( u32 );
int fputc(int , FILE *);
void USART1_SendByte( u8 );
void USART1_SendBuffer( u8 *, u16 );


#endif /* __USARTDRV_H */
