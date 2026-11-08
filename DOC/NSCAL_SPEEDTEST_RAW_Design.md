# N SCAL SPEEDTEST RAW 列车测速系统设计文档

## 文档信息

| 项目 | 内容 |
|---|---|
| 项目名称 | N SCAL SPEEDTEST RAW |
| 文档名称 | RAW 版列车测速系统设计文档 |
| 版本 | V1.0 |
| 日期 | 2026-11-07 |
| MCU | STM32F103C8T6 / HK32F103C8T6 |
| 开发环境 | Keil MDK 5，Arm Compiler 5，MicroLIB |
| 文档状态 | 设计说明，包含当前实现与待整改项 |

## 1. 概述

### 1.1 项目背景

本工程由 `N SCAL SPEEDTEST` 复制而来，保留双红外对管测速、消抖、超时归零等功能，但取消了原工程中的车型参数表和两段折线映射。

RAW 版本把测得的模型速度按固定 N 比例 **1:160** 放大，再换算成 **km/h** 发送给 Nextion 屏幕。屏幕端可自行把 km/h 换算回模型 mm/s。

### 1.2 设计目标

- 用两个相隔 30 mm 的红外传感器测量列车通过时间；
- 由时间计算模型速度 `NOWSPEED_temp`，单位为 mm/s；
- 按固定 N 比例 1:160 换算成现实速度 km/h；
- 通过 USART1 把 km/h 发送给 Nextion 屏幕；
- 抑制传感器抖动造成的重复触发；
- 一次测速后屏蔽测速，直到两个传感器都稳定离开 1.5 秒；
- 两个传感器都离开后 5 秒无车，发送 0；
- **两个红外对管的触发顺序不分先后**，PB0 或 PB1 先触发都可以测速。

**关键设计原则：** 任意一个传感器先触发都作为计时起点，另一个传感器作为终点；正向和反向通过使用同一套算法，结果取两个边沿之间的绝对时间差。

### 1.3 术语

| 术语 | 含义 |
|---|---|
| 原始速度 | `NOWSPEED_temp`，由传感器时间差算出的模型速度，单位 mm/s |
| 发送速度 | `NOWSPEED`，按 N 比例换算后的现实速度，单位 km/h |
| 捕获时间差 | `speed_capture_delta`，TIM3 的计数值差 |
| 测速屏蔽 | 一次测速完成后，暂时不接受新的测速 |
| 稳定离开 | 两个传感器都处于消抖后的高电平状态 |

## 2. 需求

### 2.1 功能需求

| 编号 | 需求 | 当前实现 |
|---|---|---|
| FR-01 | 测量两个传感器之间的通过时间，**不限制触发先后** | TIM3 10 kHz 自由运行，TIM4 1 ms 采样确认边沿 |
| FR-02 | 计算模型速度 | `NOWSPEED_temp = 300000 / speed_capture_delta` |
| FR-03 | 按 N 比例换算成现实速度 | `NOWSPEED = NOWSPEED_temp × 0.576` km/h |
| FR-04 | 屏幕显示速度 | USART1 发送 `n0.val=...` |
| FR-05 | 同时输出原始速度调试值 | 发送 `limitset.n1.val=...` |
| FR-06 | 抑制传感器抖动 | 下降沿 2 ms、上升沿 50 ms |
| FR-07 | 防止重复测速 | 测速后屏蔽，两个传感器稳定离开 1.5 秒后解除 |
| FR-08 | 无车 5 秒归零 | TIM4 1 ms 计数 5000 次后发送 0 |
| FR-09 | 不做车型映射 | 固定 N_SCALE=160，不做两段折线 |
| FR-10 | 低速仍可测量 | 10 kHz、16 位计数器，量程约 6.55 s |

### 2.2 性能与约束

- 传感器间距：30 mm；
- 原始速度实测范围：约 40～600 mm/s；
- 发送速度范围：约 23～346 km/h；
- 显示为整数，没有小数位；
- 单片机不做满量程截断，最终由屏幕限制；
- 系统不使用 RTOS，主循环轮询 + 定时器中断。

## 3. 系统总体设计

### 3.1 系统框图

```text
        PB0 / PB1 红外传感器（低电平有效）
                    │
                    ▼
        TIM4 1 ms 中断采样 + 软件消抖
                    │
                    ▼
        speed_capture_edge() 记录边沿
                    │
                    ▼
        TIM3 10 kHz 自由运行计数器
                    │
                    ▼
        speed_capture_delta（计数值差）
                    │
                    ▼
        NOWSPEED_temp = 300000 / delta  (mm/s)
                    │
                    ▼
        NOWSPEED = NOWSPEED_temp × 0.576  (km/h)
                    │
                    ▼
        USART1 9600 8N1 → Nextion 屏幕
```

### 3.2 数据流

```text
PB0/PB1 电平
  → sensor_debounce_tick()
  → 稳定下降沿
  → speed_capture_edge()
  → speed_capture_delta
  → NOWSPEED_temp (mm/s)
  → N_SCALE 160 + km/h 换算
  → NOWSPEED (km/h)
  → datasend()
  → n0.val / limitset.n1.val
```

### 3.3 运行模式

- 上电初始化；
- 主循环持续调用 `speedtest()`；
- TIM4 每 1 ms 进入中断，处理消抖、5 秒计时和稳定离开计时；
- TIM3 只作为 10 kHz 自由运行时间基准，不开启 CC3/CC4 中断；
- 一次有效测速完成后发送速度，并屏蔽测速；
- 两个传感器稳定高电平持续 1.5 秒后，才允许下一次测速；
- 两个传感器都离开后开始 5 秒无车计时，超时发送 0。

## 4. 硬件设计

### 4.1 MCU 与时钟

| 项目 | 参数 |
|---|---|
| MCU | STM32F103C8T6（兼容 HK32F103C8T6） |
| 内核 | Cortex-M3 |
| 系统时钟 | 72 MHz（HK32 分支按 56 MHz 处理） |
| 调试 | SWD |
| 开发环境 | Keil MDK 5 + ArmCC 5 |

### 4.2 引脚分配

| 引脚 | 功能 | 说明 |
|---|---|---|
| PB0 | IR_SENSE1 / TIM3_CH3 | 红外传感器 1，低电平有效，内部上拉 |
| PB1 | IR_SENSE2 / TIM3_CH4 | 红外传感器 2，低电平有效，内部上拉 |
| PA9 | USART1_TX | 接 Nextion 屏幕 RX |
| PA10 | USART1_RX | 接 Nextion 屏幕 TX |
| GND | 公共地 | 传感器、屏幕、MCU 必须共地 |

### 4.3 传感器接口

- 两个红外传感器输出为数字开关量；
- 未遮挡时为高电平，遮挡时为低电平；
- PB0/PB1 配置为 `GPIO_Mode_IPU`；
- **两个传感器没有固定的“1 在前、2 在后”安装要求**，先触发任意一个都可以；
- 传感器供电和地线要稳定，避免电机干扰；
- 如果现场干扰严重，建议增加 RC 低通和 74HC14 施密特触发器。

### 4.4 串口屏接口

- USART1 波特率：9600；
- 数据位 8，停止位 1，无校验；
- Nextion 指令以三个 `0xFF` 结束；
- 当前发送两个对象：
  - `n0.val`：按 N 比例换算后的 km/h；
  - `limitset.n1.val`：原始模型速度 mm/s，调试用。

## 5. 软件架构

### 5.1 模块划分

| 文件 | 职责 |
|---|---|
| `main.c` | 系统初始化、主循环 |
| `app.c` / `app.h` | 测速状态机、消抖、RAW 速度换算 |
| `timerdrv.c` / `timerdrv.h` | TIM3 10 kHz 时间基准、TIM4 1 ms 中断 |
| `ioset.c` / `ioset.h` | PB0/PB1 输入初始化与读取 |
| `usartdrv.c` / `usartdrv.h` | USART1 初始化、`fputc` 重定向、发送函数 |
| `datatrans_rcv.c` / `.h` | Nextion 数据发送、串口接收帧解析 |
| `stm32f10x_it.c` | TIM4 中断、USART1 接收中断、异常处理 |

### 5.2 初始化顺序

```c
SystemInit();                       // 72 MHz
USART1_Init(USART1_BAUDRATE);       // 9600
datarcvbufinit();
TIM_Interrupt_Init();               // TIM4 1 ms
app_init();                         // GPIO、消抖、默认状态
TIM3_InputCapture_Init();           // TIM3 10 kHz 自由运行
while (1) {
    speedtest();
}
```

### 5.3 主循环

`main()` 只调用 `speedtest()`。`speedtest()` 完成：

- 处理已经完成的捕获结果；
- 计算并发送 km/h；
- 根据传感器状态维护 5 秒无车计时；
- 维护“两个传感器稳定离开 1.5 秒”后重新允许测速的条件；
- 处理 5 秒超时发送 0。

## 6. 测速原理

### 6.1 两个传感器的边沿时间

两个传感器相距：

```text
D = 30 mm
```

系统不要求传感器 1 必须在传感器 2 前面，也不要求列车固定方向。  
可能是 PB0 先触发，也可能是 PB1 先触发；先触发的那个作为计时起点，另一个作为终点：

```text
speed_capture_delta = t_stop - t_start
```

其中 `t_start` 来自先触发的传感器，`t_stop` 来自后触发的另一个传感器。

### 6.2 TIM3 时间基准

TIM3 配置为 10 kHz：

```text
TIM_Prescaler = 7200 - 1
TIM_Period    = 0xFFFF
```

因此：

```text
1 个计数 = 0.1 ms = 100 us
```

16 位计数器最大计时：

```text
65535 × 0.1 ms ≈ 6.55 s
```

对应最低可测速度约：

```text
30 mm / 6.55 s ≈ 4.58 mm/s
```

实测原始速度最低约 40 mm/s，因此余量充足。

### 6.3 原始速度计算

```c
NOWSPEED_temp = 300000 / speed_capture_delta;   // mm/s
```

推导：

```text
v = 30 mm / (delta × 0.1 ms)
  = 30 / (delta × 0.0001 s)
  = 300000 / delta mm/s
```

示例：

| 原始速度 | 通过时间 | 10 kHz 计数 |
|---:|---:|---:|
| 40 mm/s | 750 ms | 7500 |
| 100 mm/s | 300 ms | 3000 |
| 300 mm/s | 100 ms | 1000 |
| 600 mm/s | 50 ms | 500 |

> 注意：`NOWSPEED_temp` 是模型速度，不是最终发送速度。

### 6.4 触发顺序无关性

- **不规定 PB0 必须先触发**；
- **不规定 PB1 必须先触发**；
- **不要求列车只能单向通过**；
- 任意一个传感器先产生稳定下降沿，就记录为起点；
- 另一个传感器产生稳定下降沿时，记录为终点；
- 两个方向的计算都使用两个边沿之间的绝对时间差。

代码中由 `speed_capture_first_sensor` 记录先触发通道，`speed_capture_edge()` 的逻辑等价于：

```c
if (第一个边沿) {
    记录起点和先触发传感器;
}
else if (另一个传感器到达) {
    计算 delta = 当前捕获值 - 起点捕获值;
    产生一次测速结果;
}
```

| 实际触发顺序 | 起点 | 终点 | 结果 |
|---|---|---|---|
| PB0 → PB1 | PB0 | PB1 | 正常测速 |
| PB1 → PB0 | PB1 | PB0 | 正常测速 |
| 两个几乎同时 | 先被软件确认者 | 另一个 | 只要时间差大于 0 即可测速 |

安装时不需要为了“方向正确”而交换传感器接线，也不需要区分哪个是“入口传感器”。

## 7. 信号采集与消抖

### 7.1 为什么不用原始捕获中断直接测速

红外传感器在列车经过时会在阈值附近抖动。如果直接把 TIM3 的 CC3/CC4 捕获中断接到测速状态机，抖动会产生多个下降沿，导致重复测速。

当前设计：

- TIM3 只作为 10 kHz 自由运行计数器；
- TIM4 每 1 ms 采样 PB0/PB1；
- 软件确认稳定状态后，才调用 `speed_capture_edge()`；
- 时间戳使用 `TIM_GetCounter(TIM3)`。

### 7.2 非对称消抖

```c
#define SENSOR_FALL_DEBOUNCE_MS 2
#define SENSOR_RISE_DEBOUNCE_MS 50
#define SENSOR_CLEAR_HOLD_MS    1500
```

| 参数 | 作用 |
|---|---|
| 下降沿 2 ms | 快速确认列车到达，保证高速量程 |
| 上升沿 50 ms | 慢速确认列车离开，抑制低速阈值抖动 |
| 稳定离开 1500 ms | 两个传感器都稳定高电平持续 1.5 秒后，才允许重新捕获 |

### 7.3 消抖状态机

每个传感器维护：

```text
sensor_last_raw   上一次原始电平
sensor_stable     当前确认后的稳定电平
sensor_count      连续相同采样计数
```

流程：

```text
raw 与 last_raw 不同 → 清零计数
raw 与 last_raw 相同 → 计数递增
计数达到 2 ms（低）或 50 ms（高）→ 更新 stable
stable 由高变低 → 产生一次确认下降沿
```

### 7.4 捕获状态机

```text
IDLE
  │ 任意一个传感器出现第一个稳定下降沿
  ▼
WAIT_SECOND
  │ 另一个传感器出现稳定下降沿
  ▼
RESULT_READY
  │ speedtest() 读取并发送
  ▼
IGNORE（两个传感器都稳定离开 1.5 秒）
  │
  ▼
IDLE
```

无论先触发的是 PB0 还是 PB1，都按“先触发者作为起点、另一个作为终点”处理；状态机不依赖固定的方向。

### 7.5 重新允许捕获条件

```c
if (sensor_clear_time >= SENSOR_CLEAR_HOLD_MS) {
    speed_capture_ignore = 0;
}
```

必须满足：

- 两个传感器都稳定高电平；
- 稳定高电平持续时间达到 1500 ms。

任意一个传感器再次被挡住时，`sensor_clear_time` 立即清零，重新开始 1.5 秒计时。

## 8. RAW 速度换算

### 8.1 固定 N 比例

本工程不做车型映射，只使用固定 N 比例：

```c
#define N_SCALE 160
```

### 8.2 换算公式

```text
模型速度：NOWSPEED_temp，单位 mm/s

现实速度(mm/s) = NOWSPEED_temp × 160
现实速度(km/h) = 现实速度(mm/s) × 3.6 / 1000
                = NOWSPEED_temp × 160 × 0.0036
                = NOWSPEED_temp × 0.576
```

代码：

```c
NOWSPEED = (NOWSPEED_temp * N_SCALE * 36 + 5000) / 10000;
```

其中 `× 36 / 10000` 就是 `× 0.0036`，`+5000` 是整数四舍五入。

### 8.3 发送值示例

| 模型实际速度 | 10 kHz 计数 | 发送的 NOWSPEED（km/h） |
|---:|---:|---:|
| 40 mm/s | 7500 | 约 23 |
| 100 mm/s | 3000 | 约 58 |
| 300 mm/s | 1000 | 约 173 |
| 600 mm/s | 500 | 约 346 |

> `NOWSPEED_temp` 仍然是 mm/s，`NOWSPEED` 才是放大并换算后的 km/h。

### 8.4 屏幕端反算

如果屏幕需要还原模型实际速度：

```text
模型 mm/s = 收到的 km/h × 125 / 72
          ≈ km/h × 1.736111
```

整数写法：

```c
model_mm_s = (km_h * 125 + 36) / 72;
```

如果屏幕要现实世界 mm/s：

```text
现实 mm/s = km/h × 2500 / 9
```

## 9. 5 秒归零与测速屏蔽

### 9.1 5 秒无车归零

TIM4 每 1 ms 中断一次，`maintaintime` 初始为 5000：

```text
5000 × 1 ms = 5 s
```

状态：

- 任一传感器被挡住：`vflg_notrain = 0`，`maintaintime = 5000`；
- 两个传感器都恢复：`vflg_notrain = 1`，开始倒计时；
- 倒计时到 0：`vflg_send_zero = 1`；
- 主循环发送一次 `NOWSPEED = 0`。

### 9.2 测速完成后的屏蔽

一次有效速度发送后，`speed_capture_ignore` 保持为 1，测速功能关闭。

重新允许捕获的条件不是“从测速完成起固定等待 1.5 秒”，而是：

```text
两个传感器都稳定离开，并且稳定高电平持续 1500 ms
```

对应代码：

```c
#define SENSOR_CLEAR_HOLD_MS 1500

if (sensor_clear_time >= SENSOR_CLEAR_HOLD_MS) {
    speed_capture_ignore = 0;
}
```

如果中间任意一个传感器再次被挡住，`sensor_clear_time` 清零，1.5 秒重新开始计算。

由于上升沿消抖为 50 ms，从物理上完全离开到重新允许测速，实际约为 1.55 秒。

### 9.3 超时与速度的互斥

- 有真实速度发送时，清除 `vflg_send_zero`；
- 任一传感器被挡住时，清除 `vflg_send_zero`；
- 避免“先发速度、紧接着又发 0”。

## 10. 通信设计

### 10.1 USART1 参数

```text
波特率：9600
数据位：8
停止位：1
校验：无
```

### 10.2 发送格式

`datasend()` 当前发送：

```c
printf("n0.val=%d\xff\xff\xff", NOWSPEED);
printf("limitset.n1.val=%d\xff\xff\xff", NOWSPEED_temp);
```

其中：

- `n0.val`：放大后的 km/h；
- `limitset.n1.val`：原始模型速度 mm/s，便于调试和标定。

每条 Nextion 指令以 `FF FF FF` 结束。

### 10.3 接收协议

`datarcev()` 设计为解析 9 字节帧：

```text
AA AA AA 0A ... 
```

当前状态：

- `datarcev()` 尚未在主循环中调用；
- 串口接收中断把字节写入 `Rx_buf`；
- `datarcvbufinit()` 清除的也是 `Rx_buf`；
- `datarcev()` 内部的复制语句目前是 `Rx_buf[i] = Rx_buf[temp_tail]`，属于自拷贝，应从环形缓冲区正确取数；
- 车型切换在 RAW 版本中不需要。

## 11. 初始化与运行流程

### 11.1 上电流程

```text
SystemInit
  → USART1_Init(9600)
  → datarcvbufinit
  → TIM4 1 ms 初始化
  → GPIO / 消抖 / 默认状态
  → TIM3 10 kHz 初始化
  → while(1) speedtest()
```

### 11.2 一次测速时序

```text
t0：任意一个传感器先出现稳定下降沿，记录 TIM3 计数
t1：另一个传感器出现稳定下降沿，计算 delta = t1 - t0
t2：speedtest() 读取 ready，计算 NOWSPEED_temp 和 NOWSPEED
t3：datasend() 发送 n0.val（km/h）
t4：保持测速屏蔽
t5：两个传感器稳定高电平持续 1500 ms 后，允许下一次捕获
```

上面的 `t0` 不一定是 PB0，也不一定是 PB1；两个方向都适用。

## 12. 标定流程

RAW 版本没有车型参数表，只需要确认：

1. 传感器间距 `DISTANCE` 为 30 mm；
2. `N_SCALE` 与实际 N 比例一致（默认 160）；
3. TIM3 为 10 kHz；
4. 最低速和最高速时 `NOWSPEED_temp` 在 40～600 mm/s 的预期范围内；
5. 屏幕端按 `km/h × 125 / 72` 还原模型 mm/s。

### 12.1 修改 N 比例

如果实际是 1:150：

```c
#define N_SCALE 150
```

如果实际是 1:148：

```c
#define N_SCALE 148
```

修改后重新计算：

```text
NOWSPEED = NOWSPEED_temp × N_SCALE × 0.0036
```

## 13. 测试计划

| 编号 | 测试项 | 方法 | 预期 |
|---|---|---|---|
| T-01 | 最低速 | 模型以最低速通过 | `NOWSPEED` 约 23 km/h |
| T-02 | 常用速度 | 低速到高速通过 | `NOWSPEED` 单调增加 |
| T-03 | 最高速 | 600 mm/s | `NOWSPEED` 约 346 km/h |
| T-04 | 反向通过 | 交换列车通过方向 | 与正向通过结果一致，不要求固定触发顺序 |
| T-05 | 重复触发 | 同一列车连续通过 | 两个传感器稳定离开 1.5 秒后才允许下一次测速 |
| T-06 | 5 秒归零 | 列车离开后等待 | 约 5 秒发送 0 |
| T-07 | 传感器抖动 | 手动快速遮挡/释放 | 不产生额外速度 |
| T-08 | 串口显示 | 9600 8N1 | `n0.val` 正常更新 |
| T-09 | 屏幕反算 | 用 km/h × 125 / 72 | 得到模型 mm/s |
| T-10 | 长时间遮挡 | 保持一个传感器遮挡 | 不重新允许测速 |

### 13.1 建议仪器

- 双通道信号发生器或可调脉冲源；
- 逻辑分析仪（观察 PB0/PB1 与 TIM3 计数）；
- 秒表/慢动作视频（验证 5 秒和 1.5 秒）；
- Nextion 屏幕实机。

## 14. 已知问题与风险

1. **`datarcev()` 未在主循环调用**：屏幕发来的命令目前不会处理。
2. **`datarcev()` 复制源写错**：`Rx_buf[i] = Rx_buf[temp_tail]` 是自拷贝，应从环形缓冲区读取；`datatrans_rcv.c` 中还声明了未使用的 `Rx_Buffer`。
3. **`TIM3_IRQHandler` 仍保留**：TIM3 CC 中断没有开启，该函数当前不会执行；后续若开启会绕过软件消抖。
4. **`datatrans_rcv.c` 中 `extern u8 NOWSPEED`**：实际定义为 `u32`，类型不一致；当前未使用但应修正。
5. **`STM32F10X_HD` 与 C8 器件不匹配**：工程使用 HD 宏和 HD startup，C8 属于中容量；建议改为 `STM32F10X_MD` + `startup_stm32f10x_md.s` 并验证。
6. **无显示上限截断**：`NOWSPEED` 可超过屏幕量程，依赖屏幕限制。
7. **显示为整数**：低速/中速没有小数位，需要屏幕端做小数显示时另行处理。
8. **10 kHz、16 位计时器量程 6.55 s**：低于约 4.58 mm/s 会溢出；实测最低 40 mm/s，当前安全。
9. **遗留变量**：`vflg_speedtesttimer`、`delta_passtime`、`IRCONTROL`、`vflg_irstaus` 已基本不参与新测速逻辑，建议清理。
10. **固定 N 比例**：RAW 版不做车型切换；不同比例需要改 `N_SCALE` 并重新编译。

## 15. 后续改进建议

- 将屏幕命令接入主循环，处理车型/比例选择；
- 增加参数保存到 Flash；
- 使用文本控件显示一位小数；
- 增加硬件 RC + 施密特触发器，进一步抑制电机干扰；
- 修正上述已知问题，补充单元测试和台架测试记录。

## 16. 附录

### 16.1 关键常量

| 常量 | 值 |
|---|---:|
| 传感器间距 `DISTANCE` | 30 mm |
| TIM3 频率 | 10 kHz |
| 每个 TIM3 计数 | 0.1 ms |
| `SENSOR_FALL_DEBOUNCE_MS` | 2 ms |
| `SENSOR_RISE_DEBOUNCE_MS` | 50 ms |
| `SENSOR_CLEAR_HOLD_MS` | 1500 ms |
| `maintaintime` | 5000 ms |
| `N_SCALE` | 160 |
| 当前发送满量程示例 | 40～600 mm/s → 23～346 km/h |

### 16.2 关键公式

```text
NOWSPEED_temp = 300000 / speed_capture_delta    // mm/s

NOWSPEED = NOWSPEED_temp × 160 × 0.0036
         = NOWSPEED_temp × 0.576                 // km/h

屏幕反算模型速度：
model_mm_s = km_h × 125 / 72                    // mm/s
```

### 16.3 构建状态

最近一次修改后的模块级验证：

| 项目 | 值 |
|---|---|
| 验证时间 | 2026-11-07 22:16 |
| 编译器 | Arm Compiler 5 |
| 验证对象 | `app.c`、`stm32f10x_it.c` |
| 结果 | 0 Error(s) |
| 说明 | 两个文件单独编译通过；发布前在 Keil 中执行一次完整 Rebuild |

### 16.4 版本记录

| 版本 | 日期 | 说明 |
|---|---|---|
| V1.0 | 2026-11-07 | RAW 版初版；取消车型映射，固定 N 比例，测速后按稳定离开 1.5 秒重新允许 |
