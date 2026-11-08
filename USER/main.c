
/**************************************
 * 文件名  ：main.c
 * 描述    ：通过串口调试软件，向板子发送数据，板子接收到数据后，立即回传给电脑。         
 * 实验平台：MINI STM32开发板 基于STM32F103C8T6
 * 库版本  ：ST3.0.0  																										  

*********************************************************/

#include "stm32f10x.h"
#include "usartdrv.h"
#include "datatrans_rcv.h"
#include "app.h"
#include "timerdrv.h"

extern void USART1_Init( u32 );
extern void datarcvbufinit( void );
extern void TIM_Interrupt_Init(void);
extern void datarcvbufinit(void);
extern void app_init(void);
extern void speedtest(void);

int main(void)
{  
	     
  SystemInit();	//配置系统时钟为 72M 
  USART1_Init( USART1_BAUDRATE ); //USART1 配置 
  datarcvbufinit();
  datarcvbufinit();
  TIM_Interrupt_Init();
  app_init();
  TIM3_InputCapture_Init();

  while (1)
  {	 
    speedtest();
  }
}




