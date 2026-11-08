# N SCAL SPEEDTEST 列车测速系统设计文档

## 文档信息

| 项目 | 内容 |
|---|---|
| 项目名称 | N SCAL SPEEDTEST |
| 文档名称 | 列车测速系统设计文档 |
| 版本 | V1.1 |
| 日期 | 2026-11-07 |
| MCU | STM32F103C8T6 / HK32F103C8T6 |
| 开发环境 | Keil MDK 5，Arm Compiler 5，MicroLIB |
| 文档状态 | 设计说明，包含当前实现、触发顺序无关说明与待整改项 |

## 1. 概述

### 1.1 项目背景

本项目使用两个红外对管传感器测量 N 比例模型列车的通过速度，并将速度值发送到 Nextion 串口屏显示。系统需要适应不同车型：不同车型的实测原始速度范围不同，显示满量程也可能不同，因此速度算法采用“车型参数表 + 两段折线映射”的方式。

### 1.2 设计目标

- 用两个相隔 30 mm 的红外传感器测量列车通过时间；
- 由时间计算模型速度 `NOWSPEED_temp`，单位为 mm/s；
- 按车型参数映射成显示速度 `NOWSPEED`；
- 通过 USART1 把速度发送到 Nextion 屏幕；
- 抑制传感器抖动造成的重复触发；
- 两个传感器都离开后 5 秒无车，发送 0；
- 支持不同车型的参数切换；
- **两个红外对管的触发顺序不分先后**，PB0 先触发或 PB1 先触发都可以正常测速。

**关键设计原则：** 系统不规定“传感器 1 必须先在前面”。任意一个传感器先触发都作为计时起点，另一个传感器作为终点；列车正向通过和反向通过使用同一套算法，结果取两个边沿之间的绝对时间差。

### 1.3 术语

| 术语 | 含义 |
|---|---|
| 原始速度 | `NOWSPEED_temp`，由传感器时间差算出的模型速度，单位 mm/s |
| 显示速度 | `NOWSPEED`，经过车型参数映射后发送给屏幕的值 |
| 捕获时间差 | `speed_capture_delta`，TIM3 的计数值差 |
| 车型参数 | 一组 raw_min、raw_max、display_max、split 等标定参数 |
| 封锁时间 | 一次测速完成后禁止再次测速的时间，当前为 1500 ms |

## 2. 需求

### 2.1 功能需求

| 编号 | 需求 | 当前实现 |
|---|---|---|
| FR-01 | 测量两个传感器之间的通过时间，**不限制触发先后** | TIM3 10 kHz 自由运行，TIM4 1 ms 采样确认边沿；先触发者作为起点 |
| FR-02 | 计算模型速度 | `NOWSPEED_temp = 300000 / speed_capture_delta` |
| FR-03 | 按车型映射显示速度 | `speed_model_table` + 两段折线 |
| FR-04 | 屏幕显示速度 | USART1 发送 `n0.val=...` |
| FR-05 | 同时输出原始速度调试值 | 发送 `limitset.n1.val=...` |
| FR-06 | 抑制传感器抖动 | 下降沿 2 ms、上升沿 50 ms、稳定离开 200 ms |
| FR-07 | 防止重复测速 | 测速后封锁 1500 ms |
| FR-08 | 无车 5 秒归零 | TIM4 1 ms 计数 5000 次后发送 0 |
| FR-09 | 不同车型切换 | `speed_model_set(index)` 接口 + 参数表 |
| FR-10 | 低速仍可测量 | 10 kHz、16 位计数器，量程约 6.55 s |

### 2.2 性能与约束

- 传感器间距：30 mm；
- 原始速度实测范围：约 40～600 mm/s；
- 当前车型显示满量程：400；
- 显示为整数，没有小数位；
- 屏幕可对超过自身上限的值做显示限制；
- 单片机不强制限制 `NOWSPEED` 的 400/450/500 上限；
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
        车型参数表两段折线映射
                    │
                    ▼
        NOWSPEED（显示速度）
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
  → NOWSPEED_temp
  → SpeedModelProfile 映射
  → NOWSPEED
  → datasend()
  → n0.val / limitset.n1.val
```

### 3.3 运行模式

- 上电初始化；
- 主循环持续调用 `speedtest()`；
- TIM4 每 1 ms 进入中断，处理消抖、5 秒计时、1.5 秒封锁；
- TIM3 只作为 10 kHz 自由运行时间基准，不开启 CC3/CC4 中断；
- 一次有效测速完成后发送速度，并进入 1.5 秒封锁；
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
  - `n0.val`：显示速度；
  - `limitset.n1.val`：原始速度调试值。

## 5. 软件架构

### 5.1 模块划分

| 文件 | 职责 |
|---|---|
| `main.c` | 系统初始化、主循环 |
| `app.c` / `app.h` | 测速状态机、消抖、车型参数、速度映射 |
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
app_init();                         // GPIO、消抖、默认车型
TIM3_InputCapture_Init();           // TIM3 10 kHz 自由运行
while (1) {
    speedtest();
}
```

### 5.3 主循环

`main()` 只调用 `speedtest()`。`speedtest()` 完成：

- 处理已经完成的捕获结果；
- 根据传感器状态维护 5 秒无车计时；
- 处理 5 秒超时发送 0；
- 维护重新允许捕获的条件。

## 6. 测速原理

### 6.1 两个传感器的边沿时间

两个传感器相距：

```text
D = 30 mm
```

系统不要求传感器 1 必须在传感器 2 前面，也不要求列车固定方向。  
可能是 PB0 先触发，也可能是 PB1 先触发；先触发的那个作为计时起点，另一个作为终点，记录两个稳定下降沿之间的 TIM3 计数值差：

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
|---|---:|---:|
| 40 mm/s | 750 ms | 7500 |
| 100 mm/s | 300 ms | 3000 |
| 300 mm/s | 100 ms | 1000 |
| 600 mm/s | 50 ms | 500 |

> 注意：`NOWSPEED_temp` 是模型速度，不是最终显示速度。

### 6.4 触发顺序无关性

这是本设计的关键约束：

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

因此：

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
#define SENSOR_CLEAR_HOLD_MS    200
```

| 参数 | 作用 |
|---|---|
| 下降沿 2 ms | 快速确认列车到达，保证高速量程 |
| 上升沿 50 ms | 慢速确认列车离开，抑制低速阈值抖动 |
| 稳定离开 200 ms | 两个传感器都稳定高电平后，才允许重新捕获 |

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
IGNORE（1.5 秒封锁 + 稳定离开 200 ms）
  │
  ▼
IDLE
```

无论先触发的是 PB0 还是 PB1，都按“先触发者作为起点、另一个作为终点”处理；状态机不依赖固定的方向。

### 7.5 重新允许捕获条件

```c
if ((speed_holdoff == 0) &&
    (sensor_clear_time >= SENSOR_CLEAR_HOLD_MS)) {
    speed_capture_ignore = 0;
}
```

必须同时满足：

- 一次测速后的 1.5 秒封锁结束；
- 两个传感器都稳定高电平持续 200 ms。

## 8. 车型参数与速度映射

### 8.1 参数结构体

```c
typedef struct
{
    u16 raw_min;           // 该车型 NOWSPEED_temp 最小值
    u16 raw_max;           // 该车型 NOWSPEED_temp 最大值
    u16 display_max;       // 该车型 NOWSPEED 满量程
    u16 split_raw;         // 低速/高速分界
    u8  split_display;     // 分界点显示值
    u8  split_min_display; // 最低速度显示值
} SpeedModelProfile;
```

### 8.2 当前参数表

```c
const SpeedModelProfile speed_model_table[] =
{
    { 40, 600, 400, 150, 10, 1 },  // 当前车型
};
```

含义：

```text
raw_min           = 40 mm/s
raw_max           = 600 mm/s
display_max       = 400
split_raw         = 150 mm/s
split_display     = 10
split_min_display = 1
```

### 8.3 两段折线公式

#### 低速段：raw_min ～ split_raw

```text
NOWSPEED = split_min_display
         + (NOWSPEED_temp - raw_min)
           × (split_display - split_min_display)
           ÷ (split_raw - raw_min)
```

当前参数：

```text
NOWSPEED = 1
         + (NOWSPEED_temp - 40) × 9 ÷ 110
```

#### 高速段：split_raw ～ raw_max

```text
NOWSPEED = split_display
         + (NOWSPEED_temp - split_raw)
           × (display_max - split_display)
           ÷ (raw_max - split_raw)
```

当前参数：

```text
NOWSPEED = 10
         + (NOWSPEED_temp - 150) × 390 ÷ 450
```

### 8.4 当前映射结果

| NOWSPEED_temp (mm/s) | NOWSPEED |
|---:|---:|
| 40 | 1 |
| 50 | 1 |
| 80 | 4 |
| 100 | 5 |
| 150 | 10 |
| 200 | 53 |
| 300 | 140 |
| 400 | 226 |
| 500 | 313 |
| 600 | 400 |

说明：

- 计算使用整数除法，小数部分截断；
- 超过 600 mm/s 后会继续按高速段斜率输出大于 400 的值；
- 单片机不做 400 上限截断，最终由屏幕限制显示。

### 8.5 车型切换

```c
speed_model_set(index);
```

- `index` 为 `speed_model_table` 的行号；
- 默认车型由 `SPEED_MODEL_DEFAULT` 指定；
- 切换后下一次测速使用新车型参数；
- 当前参数表只有一行，增加车型时直接追加：

```c
{ 50, 550, 450, 150, 10, 1 },  // 车型 B 示例
{ 60, 500, 500, 180, 12, 1 },  // 车型 C 示例
```

### 8.6 参数有效性约束

必须满足：

```text
raw_min < split_raw < raw_max
split_min_display < split_display < display_max
```

否则可能出现除零或负斜率。

## 9. 5 秒归零与 1.5 秒封锁

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

### 9.2 1.5 秒测速封锁

一次有效速度发送后：

```c
speed_holdoff = 1500;   // 1500 ms
```

TIM4 每 1 ms 递减。封锁期间：

- `speed_capture_ignore = 1`；
- 新的边沿不会进入测速状态机；
- 直到封锁结束且两个传感器稳定离开 200 ms，才重新允许捕获。

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

- `n0.val`：显示速度；
- `limitset.n1.val`：原始速度，便于调试和标定。

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
- 车型切换命令尚未接入屏幕协议。

## 11. 初始化与运行流程

### 11.1 上电流程

```text
SystemInit
  → USART1_Init(9600)
  → datarcvbufinit
  → TIM4 1 ms 初始化
  → GPIO / 消抖 / 默认车型
  → TIM3 10 kHz 初始化
  → while(1) speedtest()
```

### 11.2 一次测速时序

```text
t0：任意一个传感器先出现稳定下降沿，记录 TIM3 计数
t1：另一个传感器出现稳定下降沿，计算 delta = t1 - t0
t2：speedtest() 读取 ready，计算 NOWSPEED_temp 和 NOWSPEED
t3：datasend() 发送 n0.val
t4：speed_holdoff = 1500 ms
t5：两个传感器稳定高电平 200 ms 后，允许下一次捕获
```

上面的 `t0` 不一定是 PB0，也不一定是 PB1；两个方向都适用。

## 12. 标定流程

### 12.1 新增车型

1. 在 `speed_model_table` 增加一行；
2. 将模型放到最低速，读取 `NOWSPEED_temp`，填入 `raw_min`；
3. 将模型放到最高速，读取 `NOWSPEED_temp`，填入 `raw_max`；
4. 设定该车型的显示满量程 `display_max`；
5. 设定低速/高速分界 `split_raw`；
6. 设定分界显示值 `split_display`；
7. 设定最低显示值 `split_min_display`；
8. 重新编译并测试全速度段。

### 12.2 读取原始速度

可以通过：

- Nextion 的 `limitset.n1.val`；
- Keil Watch 窗口观察 `NOWSPEED_temp`；
- 串口调试助手查看发送内容。

### 12.3 校验

- 低速端不出现负数或除零；
- 分界点两侧连续；
- 最高速对应 `display_max`；
- 1.5 秒内不重复发送；
- 5 秒无车发送 0。

## 13. 测试计划

| 编号 | 测试项 | 方法 | 预期 |
|---|---|---|---|
| T-01 | 最低速 | 模型以最低速通过 | `NOWSPEED` 约 1 |
| T-02 | 低速段 | 40～150 mm/s | 1～10，单调增加 |
| T-03 | 分界点 | 150 mm/s 附近 | 从 10 连续过渡 |
| T-04 | 高速段 | 150～600 mm/s | 10～400 |
| T-05 | 最高速 | 600 mm/s | 400 |
| T-06 | 重复触发 | 同一列车连续通过 | 1.5 秒内只发一次 |
| T-07 | 5 秒归零 | 列车离开后等待 | 约 5 秒发送 0 |
| T-08 | 车型切换 | 调用 `speed_model_set()` | 下一次测速使用新参数 |
| T-09 | 传感器抖动 | 手动快速遮挡/释放 | 不产生额外速度 |
| T-10 | 串口显示 | 9600 8N1 | `n0.val` 正常更新 |
| T-11 | 反向通过 | 交换列车通过方向 | 与正向通过结果一致，不要求固定触发顺序 |

### 13.1 建议仪器

- 双通道信号发生器或可调脉冲源；
- 逻辑分析仪（观察 PB0/PB1 与 TIM3 计数）；
- 秒表/慢动作视频（验证 5 秒和 1.5 秒）；
- Nextion 屏幕实机。

## 14. 已知问题与风险

1. **车型切换尚未接入屏幕**：当前只有 `speed_model_set()` 接口，没有在 `datarcev()` 或按键中调用。
2. **车型选择不保存**：掉电恢复 `SPEED_MODEL_DEFAULT`；需要 Flash/EEPROM 或屏幕启动时下发。
3. **`datarcev()` 未在主循环调用**：屏幕发来的命令目前不会处理。
4. **`datarcev()` 复制源写错**：`Rx_buf[i] = Rx_buf[temp_tail]` 是自拷贝，应从环形缓冲区读取；`datatrans_rcv.c` 中还声明了未使用的 `Rx_Buffer`。
5. **`TIM3_IRQHandler` 仍保留**：TIM3 CC 中断没有开启，该函数当前不会执行；后续若开启会绕过软件消抖。
6. **`datatrans_rcv.c` 中 `extern u8 NOWSPEED`**：实际定义为 `u32`，类型不一致；当前未使用但应修正。
7. **`STM32F10X_HD` 与 C8 器件不匹配**：工程使用 HD 宏和 HD startup，C8 属于中容量；建议改为 `STM32F10X_MD` + `startup_stm32f10x_md.s` 并验证。
8. **无显示上限截断**：`NOWSPEED` 可超过 `display_max`，依赖屏幕限制。
9. **低速显示分辨率有限**：低速段显示值只有 1～10，整数屏无法显示更细的小数。
10. **10 kHz、16 位计时器量程 6.55 s**：低于约 4.58 mm/s 会溢出；实测最低 40 mm/s，当前安全。
11. **遗留变量**：`vflg_speedtesttimer`、`delta_passtime`、`IRCONTROL`、`vflg_irstaus` 已基本不参与新测速逻辑，建议清理。
12. **参数表未做运行时校验**：非法 `split_raw` 可能导致除零。

## 15. 后续改进建议

- 将车型选择接入 Nextion 按键，并在屏幕上显示当前车型；
- 将车型编号保存到内部 Flash；
- 用 TIM3 输入捕获 + 溢出扩展实现更高分辨率/更宽量程；
- 使用文本控件显示一位小数，提高低速显示分辨率；
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
| `SENSOR_CLEAR_HOLD_MS` | 200 ms |
| `speed_holdoff` | 1500 ms |
| `maintaintime` | 5000 ms |
| 当前车型 `raw_min` | 40 mm/s |
| 当前车型 `raw_max` | 600 mm/s |
| 当前车型 `display_max` | 400 |
| 当前车型 `split_raw` | 150 mm/s |
| 当前车型 `split_display` | 10 |
| 当前车型 `split_min_display` | 1 |

### 16.2 关键公式

```text
NOWSPEED_temp = 300000 / speed_capture_delta

低速段：
NOWSPEED = split_min_display
         + (NOWSPEED_temp - raw_min)
           × (split_display - split_min_display)
           ÷ (split_raw - raw_min)

高速段：
NOWSPEED = split_display
         + (NOWSPEED_temp - split_raw)
           × (display_max - split_display)
           ÷ (raw_max - split_raw)
```

### 16.3 构建状态

最近一次全工程构建：

| 项目 | 值 |
|---|---|
| 构建时间 | 2026-11-07 00:28 |
| 编译器 | Arm Compiler 5 |
| 结果 | 0 Error(s)，0 Warning(s) |
| Code | 8820 字节 |
| RO-data | 348 字节 |
| RW-data | 80 字节 |
| ZI-data | 528 字节 |

### 16.4 版本记录

| 版本 | 日期 | 说明 |
|---|---|---|
| V1.1 | 2026-11-07 | 补充“红外对管触发顺序不分先后”的设计说明；同步接收缓冲实现、测试项和构建状态 |
| V1.0 | 2026-11-07 | 初版，基于当前工程实现整理 |
