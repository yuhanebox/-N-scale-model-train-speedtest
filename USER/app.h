#ifndef __APP_H
#define	__APP_H

#include "stm32f10x.h"
#include <stdio.h>

typedef struct
{
    u16 raw_min;          // 该车型 NOWSPEED_temp 最小值
    u16 raw_max;          // 该车型 NOWSPEED_temp 最大值
    u16 display_max;      // 该车型 NOWSPEED 满量程
    u16 split_raw;        // 低速段/高速段分界
    u8  split_display;    // 分界点显示值
    u8  split_min_display;// 最低速度显示值
} SpeedModelProfile;

#define SPEED_MODEL_DEFAULT 0

void speed_model_set(u8 index);

#define DISTANCE 30 // 传感器间距，单位 mm
#define CONFIGVAL 1333  // 保留原宏，当前速度换算不再使用

void speed_capture_edge(u8 sensor, u16 capture);
void sensor_debounce_tick(void);

#endif /* __APP_H */
