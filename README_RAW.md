# N SCAL SPEEDTEST RAW

这是基于 `N SCAL SPEEDTEST` 复制出来的独立工程，原工程未改动。

## 与旧工程的区别

旧工程：

```text
NOWSPEED_temp（模型实际速度 mm/s）
  → 车型参数表
  → 两段折线映射
  → NOWSPEED（显示速度）
  → 串口屏
```

RAW 工程：

```text
NOWSPEED_temp（模型实际速度 mm/s）
  → × 160（N 比例）
  → × 0.0036（mm/s 转 km/h）
  → NOWSPEED = NOWSPEED_temp × 0.576（km/h）
  → 串口屏
```

即：

- 不做车型参数表映射；
- 不做低速/高速分段；
- 不做满量程截断；
- 固定按 N 比例 **1:160** 放大；
- 直接把放大后的现实速度按 **km/h** 发送。

## 换算公式

```text
模型速度：NOWSPEED_temp，单位 mm/s

现实速度(mm/s) = NOWSPEED_temp × 160
现实速度(km/h) = 现实速度(mm/s) × 3.6 / 1000
                = NOWSPEED_temp × 160 × 0.0036
                = NOWSPEED_temp × 0.576
```

代码：

```c
#define N_SCALE 160

NOWSPEED = (NOWSPEED_temp * N_SCALE * 36 + 5000) / 10000;
```

其中 `× 36 / 10000` 就是 `× 0.0036`，`+5000` 是整数四舍五入。

## 发送值示例

| 模型实际速度 | 10 kHz 计数 | 发送的 NOWSPEED（km/h） |
|---:|---:|---:|
| 40 mm/s | 7500 | 约 23 |
| 100 mm/s | 3000 | 约 58 |
| 300 mm/s | 1000 | 约 173 |
| 600 mm/s | 500 | 约 346 |

> `NOWSPEED_temp` 仍然是 mm/s，`NOWSPEED` 才是放大并换算后的 km/h。

## 代码改动

- `USER/app.c`：
  - 删除 `speed_model_table`；
  - 删除 `speed_model_set()` 和 `g_speed_model`；
  - 测速后按 `NOWSPEED = NOWSPEED_temp × 0.576` 计算；
  - `datasend(NOWSPEED, NOWSPEED_temp)`。
- `USER/app.h`：
  - 删除 `SpeedModelProfile` 和相关宏；
  - 增加 `N_SCALE 160`；
  - 保留 `speed_capture_edge()`、`sensor_debounce_tick()`。

## 保留的功能

- TIM3 10 kHz 自由运行计时；
- TIM4 1 ms 采样；
- 红外对管触发顺序不分先后；
- 下降沿 2 ms、上升沿 50 ms 消抖；
- 测速完成后屏蔽测速，直到两个传感器都稳定离开持续 1.5 秒才重新允许；
- 两个传感器离开后 5 秒发送 0；
- 串口屏发送 `n0.val` 和 `limitset.n1.val`。

## 串口屏

USART1 参数：

```text
9600 8N1
```

发送内容：

```text
n0.val=<NOWSPEED><FF FF FF>          // 放大后的 km/h
limitset.n1.val=<NOWSPEED_temp><FF FF FF>  // 原始 mm/s，调试用
```

`n0.val` 是 km/h 显示值；`limitset.n1.val` 保留原始模型速度，方便标定和排查。

## 打开工程

用 Keil 打开：

```text
USER/NSCALSPEEDTEST.uvprojx
```

工程 target 名称仍是 `NSCALSPEEDTEST`，但目录已经是独立的 `N SCAL SPEEDTEST_RAW`。

## 编译状态

`app.c`、`main.c`、`stm32f10x_it.c` 已用 ArmCC 5 单独编译通过，0 错误。  
首次在 Keil 中打开后建议执行一次 `Rebuild`，确认完整链接和下载配置。

## 说明

同目录下的 `DOC/` 是从旧工程复制过来的设计文档，内容仍包含车型映射设计；RAW 版本以本 README 和当前代码为准。
