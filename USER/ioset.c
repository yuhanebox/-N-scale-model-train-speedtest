/***************************************
 * 文件名  ：ioset.c
 * 描述    ：配置GPIO      
 * 实验平台：STM32开发板 基于STM32F103C8T6/HK32F103C8T6
 * 硬件连接：------------------------
 *          | PB0  - IR_SENSE1       |
 *          | PB1  - IR_SENSE2       |
 *           ------------------------
 * 库版本  ：ST3.0.0  

**********************************************************************************/
#include "stm32f10x.h"
#include "ioset.h"

void GPIO_IRsensInputs_Init(void);
u8 RET_IRSENSE1( void );
u8 RET_IRSENSE2( void );

/**
  * @brief  初始化 PB0 和 PB1 为上拉输入模式
  */
void GPIO_IRsensInputs_Init(void)
{
    /* 第一步：开启 GPIOB 的外设时钟 */
    // 因为 PB0 和 PB1 属于 GPIOB 端口，挂载在 APB2 总线上，必须首先开启其时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* 第二步：定义 GPIO 初始化结构体变量 */
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 第三步：给结构体成员赋值，指定 PB0 和 PB1 的输入参数 */
    
    // 3.1 指定配置的引脚：同时选中引脚 0 和引脚 1 (用按键位或符号 | 连接)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    
    // 3.2 指定引脚工作模式：选择“上拉输入”模式 (GPIO_Mode_IPU)
    // IPU (Input Pull-Up) 模式下，单片机内部会启动一个弱上拉电阻，使引脚默认保持高电平(3.3V)。
    // 这样接线时，按键的另一端直接接 GND。按键按下时引脚变低电平(0V)，没按下时稳定保持高电平。
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    
    // 注：输入模式下，不需要配置 GPIO_Speed（输出速度），保持默认即可。

    /* 第四步：调用库函数，将参数写入 GPIOB 的底层寄存器 */
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}
/**
  * @brief  读取红外传感器1的值
  */
u8 RET_IRSENSE1( void ) {
    u8   ret;
    if( (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) ) == Bit_SET ) {
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}
/**
  * @brief  读取红外传感器2的值
  */
u8 RET_IRSENSE2( void ) {
    u8   ret;
    if( (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) ) == Bit_SET ) {
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}
