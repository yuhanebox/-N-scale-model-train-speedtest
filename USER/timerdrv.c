/***************************************
 * 文件名  ：timerdrv.c
 * 描述    ：配置定时器        
 * 实验平台：STM32开发板 基于STM32F103C8T6/HK32F103C8T6
 * 硬件设定：------------------------
 *          | TIM4      |
 *           ------------------------
 * 库版本  ：ST3.0.0  

**********************************************************************************/
#include "stm32f10x.h"
#include "timerdrv.h"

void TIM_Interrupt_Init(void);

/**
  * @brief  初始化 TIM4，开启TIM4定时中断，在中断里控制pwm输出
  *         初始化 TIM2，开启TIM2定时中断，在中断里定时
*/
void TIM3_InputCapture_Init(void)
{
    TIM_TimeBaseInitTypeDef tim;
    TIM_ICInitTypeDef ic;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* 10 kHz，1 个计数 = 0.1 ms，16 位计数器约 6.55 s 量程 */
    #ifdef HK32F10X
    tim.TIM_Prescaler = 5600 - 1;
    #else
    tim.TIM_Prescaler = 7200 - 1;
    #endif
    tim.TIM_Period = 0xFFFF;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    tim.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &tim);

    /* PB0 = TIM3_CH3，下降沿捕获 */
    ic.TIM_Channel = TIM_Channel_3;
    ic.TIM_ICPolarity = TIM_ICPolarity_Falling;
    ic.TIM_ICSelection = TIM_ICSelection_DirectTI;
    ic.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    ic.TIM_ICFilter = 0x0F;
    TIM_ICInit(TIM3, &ic);

    /* PB1 = TIM3_CH4，下降沿捕获 */
    ic.TIM_Channel = TIM_Channel_4;
    TIM_ICInit(TIM3, &ic);

    // TIM3 这里只作为 100 kHz 自由运行时间基准；
    // 边沿确认由 TIM4 的 1 ms 采样 + sensor_debounce_tick() 完成，
    // 不开启 CC3/CC4 中断，避免原始抖动边沿直接进入测速逻辑。
    TIM_ClearITPendingBit(TIM3, TIM_IT_CC3 | TIM_IT_CC4);

    TIM_Cmd(TIM3, ENABLE);
}

void TIM_Interrupt_Init(void)
{
    // 1. 开启 TIM4 的外设时钟（TIM4 挂载在 APB1 总线上）
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    // 2. 配置时基单元：1 MHz 计数时钟，1000 个计数产生 1 ms 更新中断
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure1;
    #ifdef HK32F10X
    TIM_TimeBaseStructure1.TIM_Prescaler = 56 - 1;       // 56 MHz / 56 = 1 MHz
    #else
    TIM_TimeBaseStructure1.TIM_Prescaler = 72 - 1;       // 72 MHz / 72 = 1 MHz
    #endif
    TIM_TimeBaseStructure1.TIM_Period = 1000 - 1;     // 1000 个计数周期：1 MHz 下 1 ms
    TIM_TimeBaseStructure1.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure1.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure1);    // 初始化 TIM4
    TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
 

    // 3. 开启 TIM4 的更新中断使能
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

    // 4. 配置嵌套向量中断控制器 (NVIC)
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    NVIC_InitTypeDef NVIC_InitStructure1;
    NVIC_InitStructure1.NVIC_IRQChannel = TIM4_IRQn;     // 选中 TIM4 的中断通道
    NVIC_InitStructure1.NVIC_IRQChannelPreemptionPriority = 1; // 抢占优先级 1
    NVIC_InitStructure1.NVIC_IRQChannelSubPriority = 1;        // 子优先级 1
    NVIC_InitStructure1.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure1);

    // 5. 启动 TIM4 定时器
    TIM_Cmd(TIM4, ENABLE); 


}
