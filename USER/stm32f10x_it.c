/**
  ******************************************************************************
  * @file    Project/Template/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.0.0
  * @date    04/06/2009
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2009 STMicroelectronics</center></h2>
  */ 

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "stm32f10x.h"
#include "datatrans_rcv.h"
#include "app.h"

extern uint8_t Rx_buf[RX_BUFFER_SIZE];
volatile uint16_t Rx_Head = 0; // 写入指针（中断用）
volatile uint16_t Rx_Tail = 0; // 读取指针（主循环用）

extern u8 vflg_speedtesttimer;
extern u32 delta_passtime;
extern u32 maintaintime;
extern u8 vflg_notrain;
extern volatile u8 vflg_send_zero;

/** @addtogroup Template_Project
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/
/**
  * @brief  This function handles USART1 exception.
  * @param  None
  * @retval : None
  */
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        // 1. 读取数据，存入 Buffer 的头部
        Rx_buf[Rx_Head] = USART_ReceiveData(USART1);
        
        // 2. 移动头部指针，如果到末尾了就折返回 0（环形结构）
        Rx_Head = (Rx_Head + 1) % RX_BUFFER_SIZE;
        
        // 3. 注意：硬件会自动清除 RXNE 标志位
    }
}

void TIM4_IRQHandler( void ) {
    // 检查是否是 TIM4 的更新中断触发
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
    {
       // 必须手动清除中断标志位，否则会反复进入中断
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        if(vflg_speedtesttimer) {
            delta_passtime++; // 计时变量自增
        } else {
            delta_passtime = 1; // 如果计时标志位没被设置，重置计时变量
        }
        if( vflg_notrain ){
          if( maintaintime > 0 ) {
              maintaintime--;
              if( maintaintime == 0 ) {
                  vflg_send_zero = 1;
              }
          }
        }

        // 1 ms 采样 PB0/PB1，连续稳定 2 ms 才确认一次边沿
        sensor_debounce_tick();

        
    }
}
/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval : None
  */
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_CC3) != RESET)
    {
        TIM_ClearITPendingBit(TIM3, TIM_IT_CC3);
        speed_capture_edge(0, (u16)TIM_GetCapture3(TIM3));
    }

    if (TIM_GetITStatus(TIM3, TIM_IT_CC4) != RESET)
    {
        TIM_ClearITPendingBit(TIM3, TIM_IT_CC4);
        speed_capture_edge(1, (u16)TIM_GetCapture4(TIM3));
    }
}

void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval : None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval : None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval : None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval : None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval : None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval : None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval : None
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval : None
  */
void SysTick_Handler(void)
{
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval : None
  */
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */ 


/******************* (C) COPYRIGHT 2009 STMicroelectronics *****END OF FILE****/
