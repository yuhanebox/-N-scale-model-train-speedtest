#ifndef __APP_H
#define __APP_H

#include "stm32f10x.h"
#include <stdio.h>

/* RAW 版本：不做车型映射，按 N 比例 1:160 换算成 km/h。 */

#define DISTANCE 30 // 传感器间距，单位 mm
#define N_SCALE 160 // N 比例

void speed_capture_edge(u8 sensor, u16 capture);
void sensor_debounce_tick(void);

#endif /* __APP_H */
