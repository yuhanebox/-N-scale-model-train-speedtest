#ifndef __DATATRANS_RCV_H
#define	__DATATRANS_RCV_H

#include "stm32f10x.h"
#include <stdio.h>

#define RX_BUFFER_SIZE 9
void datasend( u32, u32 );
void datarcev(void);
void datarcvbufinit(void);

#endif /* __DATATRANS_RCV_H */

