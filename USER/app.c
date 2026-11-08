/***************************************
 * 文件名  ：app.c
 * 描述    ：app      
 * 实验平台：STM32开发板 基于STM32F103C8T6/HK32F103C8T6
 * 硬件连接：------------------------
 *        
 *           ------------------------
 * 库版本  ：ST3.0.0  

**********************************************************************************/
/* RAW 版本：不做车型映射，按 N 比例换算成 km/h 后发送。 */
#include "stm32f10x.h"
#include "app.h"
#include "ioset.h"


u32 NOWSPEED;
u32 NOWSPEED_temp;
volatile u8 IRCONTROL;
volatile u8 vflg_speedtesttimer;
volatile u8 vflg_irstaus;
volatile u8 vflg_notrain;
volatile u8 vflg_send_zero;
volatile u8 ir1_status;
volatile u8 ir2_status;
volatile u32 delta_passtime;
volatile u32 maintaintime;

volatile u8 speed_capture_waiting;
volatile u8 speed_capture_ignore;
volatile u8 speed_capture_first_sensor;
volatile u8 speed_capture_ready;
volatile u16 speed_capture_start;
volatile u16 speed_capture_delta;

void Delay_ms( uint32_t );
extern void datasend( u32, u32 );

void speed_capture_edge(u8 sensor, u16 capture)
{
    if( speed_capture_ignore ) {
        return;
    }

    if( !speed_capture_waiting ) {
        speed_capture_waiting = 1;
        speed_capture_first_sensor = sensor;
        speed_capture_start = capture;
    }
    else if( sensor != speed_capture_first_sensor ) {
        u16 dt = (u16)(capture - speed_capture_start);

        if( dt > 0 ) {
            speed_capture_delta = dt;
            speed_capture_ready = 1;
        }

        speed_capture_waiting = 0;
        speed_capture_ignore = 1;
    }
}

#define SENSOR_FALL_DEBOUNCE_MS 2
#define SENSOR_RISE_DEBOUNCE_MS 50
#define SENSOR_CLEAR_HOLD_MS 1500

static u8 sensor_last_raw[2] = {1, 1};
static u8 sensor_stable[2] = {1, 1};
static u8 sensor_count[2] = {0, 0};
volatile u16 sensor_clear_time;

static void sensor_debounce_init(void)
{
    sensor_last_raw[0] = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0);
    sensor_last_raw[1] = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1);
    sensor_stable[0] = sensor_last_raw[0];
    sensor_stable[1] = sensor_last_raw[1];
    sensor_count[0] = 0;
    sensor_count[1] = 0;
    sensor_clear_time = 0;
}

void sensor_debounce_tick(void)
{
    u8 raw[2];
    u8 i;

    raw[0] = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0);
    raw[1] = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1);

    for( i = 0; i < 2; i++ ) {
        u8 need_time;

        if( raw[i] != sensor_last_raw[i] ) {
            sensor_last_raw[i] = raw[i];
            sensor_count[i] = 0;
        }
        else if( sensor_count[i] < 255 ) {
            sensor_count[i]++;
        }

        // 下降沿用 2 ms 快速确认；上升沿用 20 ms 慢速确认，
        // 避免低速经过时传感器在阈值附近抖动产生第二次测速
        need_time = (raw[i] == 0) ? SENSOR_FALL_DEBOUNCE_MS
                                  : SENSOR_RISE_DEBOUNCE_MS;

        if( (sensor_count[i] >= need_time) &&
            (sensor_stable[i] != raw[i]) ) {
            sensor_stable[i] = raw[i];

            if( raw[i] == 0 ) {
                speed_capture_edge(i, (u16)TIM_GetCounter(TIM3));
            }
        }
    }

    if( (sensor_stable[0] == 1) && (sensor_stable[1] == 1) ) {
        if( sensor_clear_time < 2000 ) {
            sensor_clear_time++;
        }
    }
    else {
        sensor_clear_time = 0;
    }
}

void speedtest(void)
{
    ir1_status = RET_IRSENSE1();
    ir2_status = RET_IRSENSE2();

    // TIM3 输入捕获完成后立即处理，不能等到两个传感器都离开
    if( speed_capture_ready ) {
        speed_capture_ready = 0;

        if( speed_capture_delta > 0 ) {
            /* TIM3 计数频率 10 kHz，1 个计数 = 0.1 ms
               30 mm / (delta × 0.1 ms) = 300000 / delta mm/s */
            NOWSPEED_temp = 300000 / speed_capture_delta;

            /* RAW 版本：不做车型映射。
               模型速度 mm/s × 160 = 现实速度 mm/s；
               再乘 3.6 / 1000 换算成 km/h：
               NOWSPEED = NOWSPEED_temp × 160 × 0.0036
                        = NOWSPEED_temp × 0.576 */
            NOWSPEED = (NOWSPEED_temp * N_SCALE * 36 + 5000) / 10000;

            datasend(NOWSPEED, NOWSPEED_temp);

            // 测速完成后不按固定时间解锁；等两个传感器都稳定离开 1.5 秒再重新允许测速
            // speed_capture_ignore 已由 speed_capture_edge() 置 1

            // 有真实速度时取消之前的超时 0 标志，避免连续发送速度后又发 0
            vflg_send_zero = 0;
        }
    }

    // 只要有传感器被挡住，就取消 5 秒无车计时
    if( (ir1_status == 0) || (ir2_status == 0) ) {
        vflg_notrain = 0;
        maintaintime = 5000; // 5000 × 1 ms = 5 s
        vflg_send_zero = 0;  // 新车到来，取消上一轮的超时 0
    }
    else {
        // 两个传感器都恢复后，允许下一次边沿测量
        // 必须满足：两个传感器都稳定高电平持续 SENSOR_CLEAR_HOLD_MS（1500 ms）
        if( sensor_clear_time >= SENSOR_CLEAR_HOLD_MS ) {
            speed_capture_ignore = 0;
        }

        if( !vflg_notrain ) {
            vflg_notrain = 1;
            maintaintime = 5000; // 从完全离开开始重新计时 5 秒
        }
    }

    // 5 秒超时由 TIM4 中断置位，这里只负责发送一次 0
    if( vflg_send_zero ) {
        vflg_send_zero = 0;
        NOWSPEED = 0;
        NOWSPEED_temp = 0;

        // 超时后清掉未完成的边沿捕获
        speed_capture_waiting = 0;
        speed_capture_ignore = 0;

        datasend(NOWSPEED, NOWSPEED_temp);
    }
}

void app_init(void)
{
    GPIO_IRsensInputs_Init(); // 初始化红外传感器输入引脚
    sensor_debounce_init(); // 初始化消抖状态
    IRCONTROL = 1; // 初始化红外控制变量
    NOWSPEED = 0; // 初始化速度变量
    NOWSPEED_temp = 0; // 初始化速度临时变量
    vflg_speedtesttimer = 0; // 初始化标志位
    vflg_irstaus = 1; // 初始化传感器状态标志位
    vflg_send_zero = 0; // 初始化 5 秒超时发送标志
    delta_passtime = 1; // 初始化速度计时变量
    maintaintime = 5000; // 5000 × 1 ms = 5 s
    vflg_notrain = 0;

    speed_capture_waiting = 0;
    speed_capture_ignore = 0;
    speed_capture_first_sensor = 0;
    speed_capture_ready = 0;
    speed_capture_start = 0;
    speed_capture_delta = 0;
    sensor_clear_time = 0;

    datasend(0,0); // 初始化发送速度为0
}

void Delay_ms(uint32_t ms)
{
    
    while(ms--)
    {
        /*for(uint32_t i = 0; i <= 1000; i++) {
             // 在 72MHz 的主频下，内层循环大约消耗 1 毫秒
            __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); 
            __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        }*/
       // volatile 告诉编译器：老老实实给我数数，不准自作聪明优化掉！
        // 在 72MHz 主频、无优化或低优化下，数 8000 下刚好极其接近 1 毫秒
        for(volatile uint32_t i = 0; i < 8000; i++);
    }
}
