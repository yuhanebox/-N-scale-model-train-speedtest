/***************************************
 * 文件名  ：usartdrv.c
 * 描述    ：配置USART        
 * 实验平台：STM32开发板 基于STM32F103C8T6/HK32F103C8T6
 * 硬件连接：------------------------
 *          | PA9  - USART1(Tx)      |
 *          | PA10 - USART1(Rx)      |
 *           ------------------------
 * 库版本  ：ST3.0.0  

**********************************************************************************/

#include "stm32f10x.h"
#include "usartdrv.h"
#include <stdarg.h>

void USART1_Init( u32 );
int fputc(int , FILE *);
void USART1_SendByte( u8 );
void USART1_SendBuffer( u8 *, u16 );

void USART1_Init(u32 baudrate)
{
    //串口1：屏幕通信
    // 1. 开启 GPIOA 和 USART1 的外设时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_USART1, ENABLE) ;

    // 2. 配置 PA9 为复用推挽输出 (USART1_TX)
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 复用推挽
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 配置 PA10 为浮空输入或上拉输入 (USART1_RX)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // 浮空输入
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. 配置 USART1 的核心硬件参数
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = baudrate;                  // 传入的波特率 (如 9600)
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;      // 8位数据位
    USART_InitStructure.USART_StopBits = USART_StopBits_1;          // 1位停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;              // 无奇偶校验
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件流控
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;  // 开启发送和接收
    USART_Init(USART1, &USART_InitStructure);

    // 5. 【核心：接收中断通道开启】开启串口的“接收缓冲区非空”中断
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    // 6. 配置内核 NVIC 开启串口中断通道
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;             // 选中 USART1 中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;    // 优先级设为 0 (最高优先响应电脑指令)
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 7. 正式使能启动串口
    USART_Cmd(USART1, ENABLE);   
}

// 重定向 fputc 函数，让 printf 重定向到 USART1
int fputc(int ch, FILE *f) {
    // 将字符写入串口发送寄存器
    USART_SendData(USART1, (uint8_t)ch);
    
    // 等待发送寄存器清空（确保当前字符发送成功）
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    
    return ch;
}


void USART1_SendBuffer(u8 *buffer, u16 size)
{
    for (u16 i = 0; i < size; i++)
    {
        USART1_SendByte(buffer[i]); // 逐个字节发送
    }
    
    // 整个数组发完后，确保最后一个字节也彻底从物理引脚上滑出去了 (TC = 发送完成)
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
    
}

void USART1_SendByte(u8 data)
{
    // 1. 调用官方库里的原生发送函数，明确指定往 USART1 发送数据
    USART_SendData(USART1, data); 
    
    // 2. 调用官方库里的状态读取函数，死等当前字节发送完成 (TXE = 发送数据寄存器空)
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}





