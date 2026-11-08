/***************************************
 * 文件名  ：datatrans_rcv.c
 * 描述    ：配置送受信函数       
 * 实验平台：STM32开发板 基于STM32F103C8T6/HK32F103C8T6
 * 硬件连接：------------------------
 *          | PA9  - USART1(Tx)      |
 *          | PA10 - USART1(Rx)      |
 *           ------------------------
 * 库版本  ：ST3.0.0  

**********************************************************************************/
#include "stm32f10x.h"   
#include "datatrans_rcv.h"
#include <stdio.h> 

u8 i=0;
uint8_t Rx_buf[RX_BUFFER_SIZE];
u8 *a;

void datasend( u32, u32 );
void datarcev(void);
void datarcvbufinit(void);


extern u8 NOWSPEED;
extern uint8_t Rx_Buffer[RX_BUFFER_SIZE];
extern uint16_t Rx_Head; // 写入指针（中断用）
extern uint16_t Rx_Tail; // 读取指针（主循环用）

//extern u8 vflg_speedtesttimer;
//extern u32 delta_passtime;

void datasend( u32 NOWSPEED,u32 test ){

	printf("n0.val=%d\xff\xff\xff",NOWSPEED);
    printf("limitset.n1.val=%d\xff\xff\xff",test);
    //printf("n0.val=%d\xff\xff\xff",delta_passtime);
}


/**
  * @brief  从环形缓冲区中提取并解析串口屏发送的 9 字节控制指令
  * @note   适用于带有帧头 AA AA AA 的自定义串口协议
  */
void datarcev(void) {

    uint16_t unread_bytes;
    uint8_t i;
    uint16_t temp_tail;

    // 1. 计算环形缓冲区里当前一共有多少个新字节还没处理
    unread_bytes = (Rx_Head >= Rx_Tail) ? 
                    (Rx_Head - Rx_Tail) : 
                    (RX_BUFFER_SIZE - Rx_Tail + Rx_Head);

    // 2. 如果还没攒够 9 个字节，说明屏幕还没发完，直接退出等待
    if (unread_bytes < RX_BUFFER_SIZE) {
        return; 
    }

    // 3. 顺着环形轨道的当前位置，把最前面的 9 个字节按顺序剥离复制到 Rx_buf 中
    temp_tail = Rx_Tail; // 使用临时变量模拟移动，不破坏原缓冲区
    for (i = 0; i < RX_BUFFER_SIZE; i++) {
        Rx_buf[i] = Rx_buf[temp_tail];
        temp_tail = (temp_tail + 1) % RX_BUFFER_SIZE; // 环形移动指针
    }

    // 4. 严格校验指令包：前三字节为帧头 AA AA AA，第四字节为命令 0x0A
    if (Rx_buf[0] == 0xAA && Rx_buf[1] == 0xAA && Rx_buf[2] == 0xAA) {
        if (Rx_buf[3] == 0x0A) {
            
            // 5. 根据第五字节的值，切换系统运行状态
            if (Rx_buf[5] == 0x00) {
                //TRAINSTATE = TRAINSTATE_RUNNING;
            } 
            else if (Rx_buf[5] == 0x01) {
                //TRAINSTATE = TRAINSTATE_STOP;
            }
        }
         // 0x0A ~ 0x10 均为合法命令帧；解析完成后推进全局尾指针，一次消耗这 9 个字节
         Rx_Tail = temp_tail;
         return; // 成功解析该帧，彻底退出函数
    }

    // 6. 乱码滑动清除机制：
    // 如果前三个字节不是 AA AA AA，说明遇到了错位垃圾数据。
    // 我们不能一次跳过 9 字节（可能会把错位藏在后面的有效帧漏掉），
    // 而是像滑窗一样把尾巴只往后挪 1 个字节，继续在下一轮循环中寻找下一个合法的 0xAA 帧头。
    Rx_Tail = (Rx_Tail + 1) % RX_BUFFER_SIZE;
}

void datarcvbufinit(void) {
    for( i=0;i<RX_BUFFER_SIZE;i++ ){
        Rx_buf[i]=0x00;
    }
}
