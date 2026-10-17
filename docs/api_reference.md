# 模块 API 与实现说明

本文档按模块列出对外接口、状态结构和使用方式，并说明关键实现逻辑。

## 接口速查表

下表只列出公开接口和简短说明。参数、前置条件和实现细节见后文各模块章节。

| 模块 | 接口 | 简述 |
|---|---|---|
| PID | `Pid_Init` | 初始化 `Kp`、`Ki` 和输出限幅 |
| PID | `Pid_Reset` | 复位输出累加值和上次误差 |
| PID | `Pid_SetGains` | 修改 `Kp`、`Ki` |
| PID | `Pid_SetOutputLimits` | 修改输出限幅并约束当前输出 |
| PID | `Pid_SetOutput` | 手动设置当前输出，用于无扰启用 |
| PID | `Pid_Update` | 执行一次增量式 PI，返回绝对输出 |
| Filter | `Lpf1_Init` | 初始化一阶低通系数 |
| Filter | `Lpf1_Reset` | 复位一阶低通状态 |
| Filter | `Lpf1_Update` | 输入一个样本，返回滤波值 |
| Filter | `MovAvg_Init` | 绑定外部缓冲区并初始化滑动平均 |
| Filter | `MovAvg_Reset` | 清空滑动平均窗口 |
| Filter | `MovAvg_Update` | 输入一个样本，返回当前平均值 |
| Filter | `Filter_Median3` | 求三个浮点数的中值 |
| Filter | `Filter_Median3U16` | 求三个无符号整数的中值 |
| HRTIM | `HrtimPwm_Init` | 绑定 HRTIM 句柄并配置逻辑通道 |
| HRTIM | `HrtimPwm_Start` | 启动单个 PWM 通道 |
| HRTIM | `HrtimPwm_StartAll` | 同步启动全部已配置通道 |
| HRTIM | `HrtimPwm_Stop` | 停止单个 PWM 通道 |
| HRTIM | `HrtimPwm_StopAll` | 停止全部通道并恢复到最小占空比 |
| HRTIM | `HrtimPwm_SetDuty` | 限幅并写入指定通道占空比 |
| HRTIM | `HrtimPwm_GetDuty` | 获取指定通道当前占空比 |
| HRTIM | `HrtimPwm_SetPeriod` | 修改周期并按当前占空比重算比较值 |
| HRTIM | `HrtimPwm_GetPeriod` | 获取指定通道当前周期 |
| HRTIM | `HrtimPwm_DutyToCompare` | 占空比转换为比较值，不写寄存器 |
| HRTIM | `HrtimPwm_PeriodFromFreq` | 频率转换为 HRTIM 周期值 |
| HRTIM | `HrtimPwm_EnableOutputs` | 只开输出，计数器继续运行 |
| HRTIM | `HrtimPwm_DisableOutputs` | 只关输出，计数器继续运行 |
| HRTIM | `HrtimPwm_SyncPreload` | 强制一次软件更新，把预装载值搬进有效寄存器 |
| ADC | `AdcSampler_Init` | 绑定 ADC、DMA 缓冲和通道标定参数 |
| ADC | `AdcSampler_Start` | 启动 ADC DMA 循环采样 |
| ADC | `AdcSampler_Stop` | 停止 ADC DMA |
| ADC | `AdcSampler_Update` | 解析最近一帧完整数据并更新快照 |
| ADC | `AdcSampler_Get` | 获取指定通道工程量 |
| ADC | `AdcSampler_GetRaw` | 获取指定通道原始值 |
| ADC | `AdcSampler_HasData` | 是否收到过有效数据 |
| ADC | `AdcSampler_IsUpdated` | 本次更新是否解析到新帧 |
| ADC | `AdcSampler_UpdateFrame` | 由 ADC DMA 完成回调显式指定刚完成的帧 |
| UART | `UartDebug_Init` | 绑定 UART 和发送缓冲区 |
| UART | `UartDebug_Write` | 写入原始字节并尝试启动发送 |
| UART | `UartDebug_WriteString` | 发送字符串 |
| UART | `UartDebug_Printf` | 格式化打印 |
| UART | `UartDebug_TxCpltCallback` | 在 UART 发送完成回调中调用 |
| UART | `UartDebug_Process` | 主循环中处理发送重试 |
| UART | `UartDebug_GetLost` | 获取缓冲区满导致的丢字节数 |
| OLED | `Oled_Init` | 绑定 I2C 句柄并初始化屏幕 |
| OLED | `Oled_Clear` | 清屏 |
| OLED | `Oled_ShowChar` | 在指定位置显示一个字符 |
| OLED | `Oled_ShowString` | 显示字符串 |
| OLED | `Oled_ShowUInt` | 显示无符号整数 |
| OLED | `Oled_ShowFloat` | 显示浮点数 |

---

## 1. 使用约定

- 算法层（`modules/algorithm`）不依赖 HAL，可在主机或任意 MCU 工程中编译。
- 驱动层（`modules/driver`）依赖 `stm32g4xx_hal.h`，但通过句柄注入与具体 CubeMX 工程解耦。
- 所有模块不使用动态内存。需要缓冲区的模块由调用方提供存储区。
- 所有状态都保存在调用方定义的结构体中，不使用模块内全局状态。
- 所有输出限幅、占空比范围等安全边界必须由调用方显式配置。

---

## 2. PID：`power_pid`

### 2.1 功能

增量式 PI 控制器：

```text
Δu = Kp * (e(k) - e(k-1)) + Ki * e(k)
u  = clamp(u + Δu, out_min, out_max)
```

- 无 `dt`，适用于固定控制周期；
- `Ki` 为每个控制周期的积分系数；
- 改变控制频率后，`Ki` 需要重新整定；
- 没有独立积分项，输出 `output` 本身就是累加状态。

### 2.2 数据结构

```c
typedef struct
{
    float kp;
    float ki;
    float out_min;
    float out_max;
    float output;
    float prev_error;
} Pid;
```

### 2.3 API

| 函数 | 说明 |
|---|---|
| `Pid_Init(pid, kp, ki, out_min, out_max)` | 配置增益和输出范围，并把输出复位到 `clamp(0, out_min, out_max)` |
| `Pid_Reset(pid)` | 输出复位到 `clamp(0, out_min, out_max)`，`prev_error` 清零 |
| `Pid_SetGains(pid, kp, ki)` | 运行时修改 PI 增益 |
| `Pid_SetOutputLimits(pid, out_min, out_max)` | 修改输出限幅，并把当前输出夹回新范围 |
| `Pid_SetOutput(pid, output)` | 手动设置当前输出，用于使能无扰或模式切换 |
| `Pid_Update(pid, setpoint, measurement)` | 执行一次 PI，返回累加后的绝对输出 |

### 2.4 实现说明

- `Pid_Update` 内部先计算误差 `error = setpoint - measurement`；
- 再计算增量 `delta = kp * (error - prev_error) + ki * error`；
- 保存 `prev_error = error`；
- 最后把 `output + delta` 限幅后写回 `output` 并返回。
- 第一次调用时 `prev_error` 为 0，因此初始动作相当于一次 `Kp * e + Ki * e`；
- 如果不希望初始阶跃，可在第一次 `Pid_Update` 前调用 `Pid_SetOutput` 设置当前输出。

---

## 3. Filter：`power_filter`

### 3.1 功能

提供三种轻量滤波原语：

- 一阶低通 `Lpf1`；
- 滑动平均 `MovAvg`；
- 三点中值 `Filter_Median3` / `Filter_Median3U16`。

### 3.2 一阶低通 `Lpf1`

```c
typedef struct
{
    float alpha;
    float y;
    uint8_t ready;
} Lpf1;
```

| 函数 | 说明 |
|---|---|
| `Lpf1_Init(lpf, alpha)` | 初始化滤波系数，`alpha` 限制在 0~1 |
| `Lpf1_Reset(lpf)` | 清零输出和历史状态 |
| `Lpf1_Update(lpf, x)` | 迭代 `y += alpha * (x - y)`，返回滤波值 |

实现语义：

- `alpha <= 0`：直通，输出等于输入；
- `alpha >= 1`：立即跟随输入；
- `0 < alpha < 1`：正常低通；
- 第一次调用时直接把 `y` 初始化为输入 `x`。

近似关系：

```text
alpha = dt / (tau + dt)
```

其中 `dt` 为调用周期，`tau` 为期望时间常数。由于当前 PID 已取消 `dt`，滤波器的 `alpha` 需要调用方根据实际控制周期计算。

### 3.3 滑动平均 `MovAvg`

```c
typedef struct
{
    float *buf;
    uint16_t size;
    uint16_t index;
    uint16_t count;
    float sum;
} MovAvg;
```

| 函数 | 说明 |
|---|---|
| `MovAvg_Init(avg, buf, size)` | 绑定调用方提供的缓冲区并清空 |
| `MovAvg_Reset(avg)` | 清空窗口和累加和 |
| `MovAvg_Update(avg, x)` | 环形窗口更新并返回当前平均值 |

实现说明：

- 存储区由调用方提供，不调用 `malloc`；
- 窗口未满时按当前样本数求平均；
- 窗口满后减去最旧值、加入最新值；
- 适用于变化较慢、可接受相位滞后的反馈量。

### 3.4 中值滤波

| 函数 | 说明 |
|---|---|
| `Filter_Median3(a, b, c)` | 三个 `float` 的中值 |
| `Filter_Median3U16(a, b, c)` | 三个 `uint16_t` 的中值 |

适合抑制 ADC 单次脉冲干扰，常用于与低通组合使用。

---

## 4. HRTIM PWM：`hrtim_pwm`

### 4.1 功能

把 HRTIM A~F 六个 Timer 抽象成逻辑 PWM 通道，提供：

- 占空比写入；
- 单通道和多通道同步启动/停止；
- 周期配置；
- 占空比到比较值的换算。

### 4.2 通道配置结构

```c
typedef struct
{
    uint32_t timer_index;
    uint32_t timer_id;
    uint32_t output1;
    uint32_t output2;
    uint8_t configure_waveform;
    float init_duty;
    float min_duty;
    float max_duty;
} HrtimPwmChannelCfg;
```

字段说明：

| 字段 | 说明 |
|---|---|
| `timer_index` | `HRTIM_TIMERINDEX_TIMER_A` ~ `_F` |
| `timer_id` | `HRTIM_TIMERID_TIMER_A` ~ `_F` |
| `output1` | 主输出，例如 `HRTIM_OUTPUT_TA1` |
| `output2` | 互补输出，例如 `HRTIM_OUTPUT_TA2`；填 0 表示单端 |
| `configure_waveform` | 为 1 时由模块配置 CMP1 和标准边沿输出 |
| `init_duty` | 初始占空比 |
| `min_duty` / `max_duty` | 占空比安全范围，必须在 0~1 内 |

### 4.3 API

| 函数 | 说明 |
|---|---|
| `HrtimPwm_Init(pwm, hhrtim, cfg, count)` | 绑定 HRTIM 句柄，校验配置，读取周期，设置初始比较值和输出模式 |
| `HrtimPwm_Start(pwm, channel)` | 启动单个通道，先开输出再启动计数器 |
| `HrtimPwm_StartAll(pwm)` | 同步启动所有未启动通道，适合三相/多相 |
| `HrtimPwm_Stop(pwm, channel)` | 关闭指定通道输出并停止计数器 |
| `HrtimPwm_StopAll(pwm)` | 关闭所有通道，并把占空比恢复到最小安全值 |
| `HrtimPwm_SetDuty(pwm, channel, duty)` | 占空比限幅后写入 CMP1 |
| `HrtimPwm_GetDuty(pwm, channel)` | 获取当前缓存的占空比 |
| `HrtimPwm_SetPeriod(pwm, channel, period)` | 修改周期，并按当前占空比重算比较值 |
| `HrtimPwm_GetPeriod(pwm, channel)` | 获取缓存周期 |
| `HrtimPwm_DutyToCompare(pwm, channel, duty)` | 计算比较值，不写寄存器 |
| `HrtimPwm_PeriodFromFreq(freq_hz, hrtim_clock_hz)` | 由频率和 HRTIM 时钟计算周期值 |

### 4.4 实现说明

- 标准输出配置为：输出置位于 `TIMPER`，复位位于 `TIMCMP1`；
- `output2` 的置位/复位源配置为 `NONE`，期望由 HRTIM 死区逻辑生成互补波形；
- 启动顺序为：`HAL_HRTIM_WaveformOutputStart` → `HAL_HRTIM_WaveformCountStart`；
- 若启动计数器失败，模块会回滚已经开启的输出；
- `StartAll` 通过位掩码一次性启动多个 Timer，减少多相之间的相位偏差；
- `StopAll` 会调用 `SetDuty` 把每个通道恢复到 `min_duty`。

**重要前置条件：**

HRTIM 的时钟、Prescaler、Period、Preload、DeadTime、Reset Trigger 等由 CubeMX 配置。模块只负责运行期的比较值、输出启停和周期修改。互补输出必须由 CubeMX 使能 `DeadTimeInsertion`。

### 4.5 封波的两种语义

`Stop` / `StopAll` 会关闭输出**并停止计数器**；`EnableOutputs` / `DisableOutputs` 只开关输出，计数器继续运行。**保护动作必须分清这两者。** 当 ADC 由 HRTIM 触发时（控制环放在 ADC 回调里），必须用 `DisableOutputs`：计数器一停，ADC 触发和控制回调同时停，控制环就失去了判断故障是否恢复的节拍。

### 4.6 预装载与 `SyncPreload`

CubeMX 使能 Preload 后，写入 CMP1 的值先进预装载寄存器，要等一次更新事件才生效。初始化阶段写完比较值、开输出之前，必须调一次 `HrtimPwm_SyncPreload`，否则第一个开关周期会用复位值（CMP1 = 0）导致整周期导通。

```c
HrtimPwm_SetDuty(&g_pwm, 0U, DUTY_MIN);
HrtimPwm_SyncPreload(&g_pwm);      /* 先把预装载搬进有效寄存器 */
HrtimPwm_StartAll(&g_pwm);
HrtimPwm_DisableOutputs(&g_pwm);   /* 计数器继续跑，输出先封住 */
```

---

## 5. ADC Sampler：`adc_sampler`

### 5.1 功能

面向 HRTIM 触发、DMA 循环搬运的多通道 ADC 采样模块，提供：

- DMA 帧解析；
- 原始值到工程量的标定；
- 每通道可选一阶低通；
- 最新完整帧快照；
- 新帧标志和是否有数据判断。

### 5.2 数据结构

```c
typedef struct
{
    float scale;
    float offset;
    float lpf_alpha;
    Lpf1 lpf;
} AdcSamplerChannel;
```

每个通道的工程量换算：

```text
value = raw * scale + offset
```

`lpf_alpha` 传给 `Lpf1`，用于可选的采样滤波。

```c
typedef struct
{
    ADC_HandleTypeDef *hadc;
    uint16_t *dma_buf;
    uint16_t frame_count;
    uint16_t channel_count;
    AdcSamplerChannel ch[ADC_SAMPLER_MAX_CHANNELS];
    uint16_t raw[ADC_SAMPLER_MAX_CHANNELS];
    float value[ADC_SAMPLER_MAX_CHANNELS];
    uint32_t last_frame;
    uint8_t has_data;
    uint8_t updated;
    uint8_t started;
} AdcSampler;
```

DMA 缓冲区布局：

```text
[frame0_ch0, frame0_ch1, ..., frame0_chN,
 frame1_ch0, frame1_ch1, ..., frame1_chN,
 ...]
```

每帧通道顺序必须与 CubeMX Rank 顺序一致。

### 5.3 API

| 函数 | 说明 |
|---|---|
| `AdcSampler_Init(sampler, hadc, buf, frame_count, channel_count, channels)` | 绑定 ADC 和 DMA 缓冲，配置每通道标定和低通 |
| `AdcSampler_Start(sampler)` | 启动 ADC DMA 循环采样 |
| `AdcSampler_Stop(sampler)` | 停止 ADC DMA |
| `AdcSampler_Update(sampler)` | 解析最近一帧完整数据，更新原始值和工程量快照 |
| `AdcSampler_Get(sampler, channel)` | 获取工程量 |
| `AdcSampler_GetRaw(sampler, channel)` | 获取原始 ADC 值 |
| `AdcSampler_HasData(sampler)` | 是否至少收到过一帧有效数据 |
| `AdcSampler_IsUpdated(sampler)` | 本次 `Update` 是否解析到新帧 |

### 5.4 实现说明

- `Update` 读取 DMA 剩余计数 `__HAL_DMA_GET_COUNTER`，计算已经写完的完整帧数；
- 使用“最近一帧完整数据”，避免读到 DMA 正在写入的半帧；
- 如果帧号和上次相同，则不重复更新；
- 只在解析到新帧时更新 `raw`、`value` 和滤波状态；
- `updated` 每次 `Update` 开始清零，解析到新帧后置 1；
- 模块假设 DMA 为循环模式，缓冲区类型为 `uint16_t`，DMA 数据宽度为 Half Word。

**ADC 校准不在模块内完成**，需要在启动 DMA 前调用：

```c
HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
```

### 5.5 两种取数方式

| 调用位置 | 用哪个接口 | 原因 |
|---|---|---|
| 周期任务（如 HRTIM 重复中断） | `AdcSampler_Update()` | 它读 DMA 剩余计数推算最近一帧完整数据 |
| ADC DMA 完成回调 | `AdcSampler_UpdateFrame(sampler, frame)` | 回调里推算不出来，必须由回调显式给帧号 |

**为什么回调里不能用 `Update`**：循环 DMA 在置起传输完成标志的同一时刻已经把 `CNDTR` 重装为初值，回调执行时“剩余计数 = 总长度”，`Update` 每次都算出 0 个完整帧直接返回，控制环会一直读到旧快照。

帧数为 1（一帧就是一次触发的全部 Rank）时，刚完成的帧号固定是 0：

```c
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    AdcSampler_UpdateFrame(&g_adc, 0U);
    vout = AdcSampler_Get(&g_adc, CH_VOUT);
    /* ... 控制计算 ... */
}
```

---

## 6. UART Debug：`uart_debug`

### 6.1 功能

非阻塞调试串口：

- 环形发送缓冲区；
- `HAL_UART_Transmit_IT` 发送；
- 发送完成回调续传；
- 满缓冲丢弃新数据并统计丢失字节；
- 格式化打印和字符串发送。

### 6.2 数据结构

```c
typedef struct
{
    UART_HandleTypeDef *huart;
    uint8_t *tx_buf;
    uint16_t tx_size;
    volatile uint16_t tx_head;
    volatile uint16_t tx_tail;
    volatile uint16_t tx_count;
    volatile uint16_t tx_chunk;
    volatile uint32_t tx_lost;
    volatile uint8_t tx_busy;
} UartDebug;
```

### 6.3 API

| 函数 | 说明 |
|---|---|
| `UartDebug_Init(dbg, huart, storage, size)` | 绑定 UART 和发送缓冲区，初始化环形队列 |
| `UartDebug_Write(dbg, data, len)` | 写入原始字节，必要时启动中断发送，返回实际入队长度 |
| `UartDebug_WriteString(dbg, str)` | 发送字符串 |
| `UartDebug_Printf(dbg, fmt, ...)` | 格式化打印，内部使用 `vsnprintf` |
| `UartDebug_TxCpltCallback(dbg)` | 在 `HAL_UART_TxCpltCallback` 中调用，推进队列并启动下一次发送 |
| `UartDebug_Process(dbg)` | 主循环中调用，处理发送启动失败后的重试 |
| `UartDebug_GetLost(dbg)` | 获取因缓冲区满而丢弃的字节数 |

### 6.4 实现说明

- `UartDebug_Write` 在临界区内写入环形缓冲区；
- 如果缓冲区满，保留旧数据，丢弃新数据，并累加 `tx_lost`；
- `UartDebug_Kick` 负责启动一次连续块发送；
- 发送完成后，`UartDebug_TxCpltCallback` 推进 `tx_tail`、减少 `tx_count`，并自动启动下一段；
- `UartDebug_Process` 用于主循环重试 `HAL_UART_Transmit_IT` 启动失败的情况；
- ARM 目标上使用 `__get_PRIMASK` / `__disable_irq` / `__set_PRIMASK` 做短临界区保护；
- 打印浮点数需要工具链支持 float printf。

**使用建议：**

- 串口格式化打印放在主循环，不要放在高频控制中断；
- 高频调试优先使用二进制帧或 VOFA+ 等工具；
- `UartDebug_TxCpltCallback` 必须挂接到用户自己的 `HAL_UART_TxCpltCallback` 中。

---

## 7. OLED 显示：`oled`

### 7.1 功能

SSD1306 / SSD1315 128x64 OLED 的 I2C 驱动：

- 内置 8x16 ASCII 字库（32~126），不需要外部字库文件；
- 按页寻址直接写屏，不做显存缓存，只有改动内容时才产生 I2C 流量；
- 提供字符、字符串、整数和浮点显示。

### 7.2 数据结构

```c
typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint16_t address;
} Oled;
```

`address` 用 HAL 要求的 8 位形式（7 位地址左移一位）。SSD1306 常见 7 位地址 0x3C，对应 `OLED_ADDR_DEFAULT` = 0x78；若无显示可试 0x7A。

### 7.3 API

| 函数 | 说明 |
|---|---|
| `Oled_Init(oled, hi2c, address)` | 绑定 I2C 句柄、写初始化序列并清屏；`address` 传 0 时用默认地址 |
| `Oled_Clear(oled)` | 8 页全部写 0 |
| `Oled_ShowChar(oled, x, page, ch)` | 在 `(x, page)` 显示一个字符，占用 page 与 page+1 |
| `Oled_ShowString(oled, x, page, str)` | 从 `(x, page)` 起连续显示，每字符宽 8 像素 |
| `Oled_ShowUInt(oled, x, page, num, width)` | 右对齐显示无符号整数，`width` 为最小字符宽度 |
| `Oled_ShowFloat(oled, x, page, num, width, decimals)` | 显示浮点数，`decimals` 为小数位数 |

### 7.4 坐标与实现说明

- 屏幕按页组织，共 8 页、每页 8 像素高；`x` 是像素列（0~127，建议按 8 递增）；
- 8x16 字库一个字符占两页，所以写字的 `page` 只能取 0~6，越界直接返回；
- 字模按“行”存放，模块内部转置成 OLED 需要的“列”格式；
- `Oled_ShowFloat` 依赖浮点 `printf`，目标工程链接时需要 `-u _printf_float`（newlib-nano 默认不支持 `%f`）；
- 从机不应答时 `HAL_I2C_Master_Transmit` 会一直等到超时（`OLED_I2C_TIMEOUT`），所以刷屏放在主循环，不要放进控制中断。

---

## 8. 模块与工程关系

| 模块 | 是否需要 HAL | 是否依赖具体 CubeMX 句柄 | 是否需要外部初始化 |
|---|---|---|---|
| PID | 否 | 否 | 否 |
| Filter | 否 | 否 | 否 |
| HRTIM PWM | 是 | 句柄由外部传入 | 是，HRTIM 基础配置 |
| ADC Sampler | 是 | 句柄由外部传入 | 是，ADC + DMA + 触发配置 |
| UART Debug | 是 | 句柄由外部传入 | 是，UART 配置和 NVIC |
| OLED | 是 | 句柄由外部传入 | 是，I2C 配置 |

具体工程对接步骤见 `docs/integration_guide.md`。
