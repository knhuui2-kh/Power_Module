# 工程对接指南

本文档说明各模块如何接入 STM32CubeMX + HAL 目标工程。当前只有 `algorithm` 和 `driver` 两层，驱动模块需要外部传入 CubeMX 生成的句柄，因此必须结合具体工程使用。

## 1. 总体接入边界

CubeMX 生成工程负责：

- 系统时钟和引脚复用；
- HRTIM 时钟、周期、Preload、DeadTime、输出配置；
- ADC 扫描、触发源、采样时间；
- DMA 循环模式、数据宽度和中断；
- UART 波特率、TX、NVIC；
- 中断优先级。

本库负责：

- 算法计算；
- HRTIM 占空比和启停；
- ADC DMA 帧解析和标定；
- UART 非阻塞环形缓冲。

接入时只修改 `main.c` 的 `USER CODE` 区域和用户应用文件，不修改 CubeMX 自动生成区。

### 1.1 CubeMX 必须配对的几项

以下三项不满足时不会编译报错，只在运行期表现为波形异常、通信失败或串口无输出，需逐一确认。

**1. HRTIM 使能 Preload 与 Repetition Update**

- 位置：Timer A 与 Timer B 的配置中分别勾选 **Preload Enable** 和 **Repetition Update**。
- 原因：预装载关闭时写入的 CMP1 立即生效。控制环在开关周期中途更新占空比，可能让本周期少一次复位事件，产生异常宽脉冲。
- 生成结果：`MX_HRTIM1_Init()` 中的 `HRTIM_PRELOAD_ENABLED` 与 `HRTIM_UPDATEONREPETITION_ENABLED`。

**2. I2C1 的 Timing 与内核时钟匹配**

- 位置：I2C1 的 Speed Mode 选标准模式（100kHz），Timing 由工具按实际内核时钟计算。
- 原因：I2C1 内核时钟默认取 PCLK1。170MHz 下若沿用按 16MHz 算出的值（如 `0x00303D5B`，PRESC=0、SCLL=91、SCLH=61），实际 SCL 约 1.1MHz，超出标准模式并违反建立/保持时间。
- 生成结果：`hi2c1.Init.Timing = 0x40B285C2`（170MHz 下约 100kHz）。

**3. 使能 USART1 全局中断**

- 位置：NVIC 中勾选 **USART1 global interrupt**。
- 原因：`UartDebug` 用 `HAL_UART_Transmit_IT` 发送，收尾依赖该中断；未使能时发送无法完成，环形缓冲只入队不出队。
- 生成结果：`stm32g4xx_hal_msp.c` 中的 `HAL_NVIC_EnableIRQ(USART1_IRQn)`，以及 `stm32g4xx_it.c` 中的 `USART1_IRQHandler`。
- 该处理函数由生成区提供，应用侧不要再定义同名函数，否则链接期重复定义。

## 2. 构建接入

两种方式，按工程的组织方式选择。

### 2.1 复制模块源码进工程

把模块的 `.h` 复制到 `Core/Inc`、`.c` 复制到 `Core/Src`。`Core/Inc` 通常已在 include 路径中，源文件按下述方式自动收集：

```cmake
file(GLOB MODULE_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/Core/Src/*.c)
list(REMOVE_ITEM MODULE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/Core/Src/main.c
    # 其余 CubeMX 生成的文件同样在此排除，避免与该子目录中已列出的重复
)
target_sources(your_target PRIVATE ${MODULE_SRC})
```

工程自包含，不依赖库所在路径。`apps/` 下的两个工程采用这种方式。

### 2.2 引用库路径

在目标工程 CMake 中 `include` 库根目录的清单：

```cmake
include(${CMAKE_SOURCE_DIR}/module_power/power_modules.cmake)

target_sources(your_target PRIVATE
    ${POWER_ALGORITHM_SOURCES}
    ${POWER_DRIVER_SOURCES}
)

target_include_directories(your_target PRIVATE
    ${POWER_INCLUDE_DIRS}
)
```

- 只使用 PID、Filter 时，可以只加入 `POWER_ALGORITHM_SOURCES` 和算法层 include 路径；
- 使用 HRTIM、ADC、UART、OLED 时，再加入 `POWER_DRIVER_SOURCES`。

模块更新时各工程无需逐个同步。

## 3. 推荐初始化顺序

以 STM32G474 为例，CubeMX 生成代码内部的 `MX_*_Init()` 名称由工程实际配置决定。推荐顺序：

```c
HAL_Init();
SystemClock_Config();

MX_GPIO_Init();
MX_DMA_Init();
MX_HRTIM1_Init();
MX_ADC1_Init();
MX_USART1_UART_Init();

/* USER CODE BEGIN 2 */

HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);

AdcSampler_Init(&g_adc, &hadc1, g_adc_dma, ADC_FRAME_COUNT,
                ADC_CHANNEL_COUNT, g_adc_channels);

HrtimPwm_Init(&g_pwm, &hhrtim1, g_pwm_cfg, PWM_CHANNEL_COUNT);

UartDebug_Init(&g_uart, &huart1, g_uart_tx, (uint16_t)sizeof(g_uart_tx));

AdcSampler_Start(&g_adc);
HrtimPwm_StartAll(&g_pwm);

/* USER CODE END 2 */
```

实际使用中不要一次启动全部功能。建议先验证 ADC 采样快照，再验证 HRTIM 波形，最后闭环。

## 4. PID 模块对接

PID 是纯算法，不依赖 HAL，也不依赖 CubeMX。

### 4.1 定义实例

```c
static Pid g_voltage_pi;
```

### 4.2 初始化

`Ki` 是每个控制周期的积分系数。固定开关频率下直接按该周期整定：

```c
Pid_Init(&g_voltage_pi, 0.05f, 0.001f, 0.02f, 0.95f);
```

如果此前使用的是带 `dt` 的连续域系数 `Ki_continuous`，需要换算：

```text
Ki_new = Ki_continuous * dt
```

### 4.3 控制中断中调用

```c
void HAL_HRTIM_RepetitionEventCallback(HRTIM_HandleTypeDef *hhrtim, uint32_t TimerIdx)
{
    if (TimerIdx != HRTIM_TIMERINDEX_TIMER_A)
    {
        return;
    }

    AdcSampler_Update(&g_adc);

    float feedback = AdcSampler_Get(&g_adc, ADC_CH_VOUT);
    float duty = Pid_Update(&g_voltage_pi, g_voltage_ref, feedback);

    HrtimPwm_SetDuty(&g_pwm, PWM_CH_A, duty);
}
```

### 4.4 无扰使能

```c
Pid_Reset(&g_voltage_pi);
Pid_SetOutput(&g_voltage_pi, HrtimPwm_GetDuty(&g_pwm, PWM_CH_A));
```

这样重新使能时，PI 输出从当前占空比开始，不会产生额外阶跃。

---

## 5. Filter 模块对接

Filter 是纯算法，可直接放在 ADC 采样链或应用层。

### 5.1 一阶低通

```c
static Lpf1 g_vout_lpf;

/* 控制周期 50us，期望时间常数 2ms */
float alpha = 0.00005f / (0.002f + 0.00005f);
Lpf1_Init(&g_vout_lpf, alpha);

float filtered = Lpf1_Update(&g_vout_lpf, raw_value);
```

注意：

- `alpha <= 0` 为直通；
- `alpha >= 1` 为立即跟随；
- 第一次调用直接取输入值。

### 5.2 滑动平均

```c
static float g_i_samples[8];
static MovAvg g_i_avg;

MovAvg_Init(&g_i_avg, g_i_samples, 8U);

float avg = MovAvg_Update(&g_i_avg, current_sample);
```

存储区必须由调用方提供。窗口越大越平滑，但相位滞后越大。

### 5.3 中值滤波

```c
uint16_t mid = Filter_Median3U16(sample_a, sample_b, sample_c);
```

常用于抑制 ADC 单点尖峰。

---

## 6. HRTIM PWM 模块对接

### 6.1 CubeMX 配置要点

以 Timer A 为例：

- HRTIM Clock 和 DLL 校准正常；
- Counting Mode：Up；
- Prescaler 根据开关频率选择；
- Period 与目标开关频率匹配；
- Preload：Enabled；
- Repetition Update：Enabled；
- 需要互补输出时使能 Dead Time Insertion；
- 输出 TA1/TA2 配置为 AF13；
- 需要 ADC 同步采样时，用 Timer A Period 事件触发 ADC Trigger。

模块不会配置上述选项，CubeMX 配置完成后才能调用 `HrtimPwm_Init`。

### 6.2 配置通道

```c
static HrtimPwm g_pwm;

static const HrtimPwmChannelCfg g_pwm_cfg[] =
{
    {
        HRTIM_TIMERINDEX_TIMER_A,
        HRTIM_TIMERID_TIMER_A,
        HRTIM_OUTPUT_TA1,
        HRTIM_OUTPUT_TA2,
        1U,        /* 由模块配置 CMP1 和标准边沿输出 */
        0.0f,      /* 初始占空比 */
        0.02f,     /* 最小占空比 */
        0.95f      /* 最大占空比 */
    }
};
```

### 6.3 初始化与启动

```c
HrtimPwm_Init(&g_pwm, &hhrtim1, g_pwm_cfg, 1U);
HrtimPwm_StartAll(&g_pwm);
```

多相应用必须使用 `HrtimPwm_StartAll()`，不要逐个调用 `HrtimPwm_Start()`。

### 6.4 运行期更新

```c
HrtimPwm_SetDuty(&g_pwm, 0U, duty);
```

比较值由模块自动计算并写入 CMP1，实际生效时间由 CubeMX 配置的 Preload 决定。

### 6.5 封波

```c
HrtimPwm_StopAll(&g_pwm);
```

`StopAll` 会关闭输出、停止计数器，并把占空比恢复到 `min_duty`。

### 6.6 只封波、不停计数器

```c
HrtimPwm_DisableOutputs(&g_pwm);   /* 关输出，计数器继续跑 */
HrtimPwm_EnableOutputs(&g_pwm);    /* 再开输出 */
```

当 ADC 由 HRTIM 触发时（控制环放在 ADC 回调里），**保护动作必须用这一对，不能用 `StopAll`**。计数器一停，ADC 触发和控制回调同时停，控制环就再也无法判断故障是否消失，只能靠主循环盲目重试。

### 6.7 开输出之前先同步预装载

```c
HrtimPwm_SetDuty(&g_pwm, 0U, DUTY_MIN);
HrtimPwm_SetDuty(&g_pwm, 1U, DUTY_MIN);
HrtimPwm_SyncPreload(&g_pwm);      /* 把预装载值搬进有效寄存器 */
HrtimPwm_StartAll(&g_pwm);         /* 起计数器（同时会开输出） */
HrtimPwm_DisableOutputs(&g_pwm);   /* 立刻封住，计数器保持运行 */
```

CubeMX 使能 Preload 后，初始化阶段写入的 CMP1 要等一次更新事件才生效。少了 `SyncPreload`，第一个开关周期会用到复位值 CMP1 = 0，导致整周期导通。

---

## 7. ADC Sampler 模块对接

### 7.1 CubeMX 配置要点

- Scan Mode：Enabled；
- Rank 顺序与 `AdcSamplerChannel` 数组顺序一致；
- External Trigger：HRTIM 对应的 ADC 触发事件；
- Continuous DMA Requests：Enabled；
- DMA Mode：Circular；
- DMA 数据宽度：Half Word；
- Overrun：Preserved；
- 调用 `HAL_ADCEx_Calibration_Start` 完成校准。

### 7.2 定义 DMA 缓冲和通道参数

```c
#define ADC_FRAME_COUNT   4U
#define ADC_CHANNEL_COUNT 3U

static uint16_t g_adc_dma[ADC_FRAME_COUNT * ADC_CHANNEL_COUNT];

static const AdcSamplerChannel g_adc_channels[ADC_CHANNEL_COUNT] =
{
    /* scale = 3.3 / 4095，按实际分压和偏置修改 */
    { 0.0008058f,  0.0f,   0.10f, { 0 } },  /* Vout */
    { 0.0008058f, -1.65f,  0.05f, { 0 } },  /* Iout */
    { 0.0008058f,  0.0f,   0.00f, { 0 } }   /* 辅助量 */
};
```

`AdcSamplerChannel` 中的 `scale`、`offset` 需要根据实际采样电路计算。

### 7.3 初始化和启动

```c
static AdcSampler g_adc;

AdcSampler_Init(&g_adc, &hadc1, g_adc_dma, ADC_FRAME_COUNT,
                ADC_CHANNEL_COUNT, g_adc_channels);

AdcSampler_Start(&g_adc);
```

### 7.4 控制中断中读取

```c
AdcSampler_Update(&g_adc);

float vout = AdcSampler_Get(&g_adc, 0U);
float iout = AdcSampler_Get(&g_adc, 1U);
```

`AdcSampler_Update` 会读取 DMA 计数，选取最近一帧完整数据，避免使用半帧。`AdcSampler_IsUpdated` 可在主循环中用于判断采样是否持续更新，实现超时保护。

### 7.5 控制环放在 ADC DMA 完成回调中

7.4 节的写法把控制环挂在周期任务上，控制节拍由 PWM 中断决定；另一种写法是把控制环直接放进 ADC DMA 完成回调，控制节拍由采样决定，采样与控制严格一一对应，也不需要额外的定时中断。

关键点：**这种写法不能用 `AdcSampler_Update`**。循环 DMA 在置起传输完成标志的同一时刻已经把 CNDTR 重装为初值，回调执行时“剩余计数 = 总长度”，`Update` 每次都判定为无新帧直接返回，控制环会一直读到旧快照。必须用 `AdcSampler_UpdateFrame` 显式给帧号：

```c
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1)
    {
        return;
    }

    AdcSampler_UpdateFrame(&g_adc, 0U);   /* 帧数为 1，刚完成的帧号固定是 0 */

    vout = AdcSampler_Get(&g_adc, CH_VOUT);
    /* ... 保护判断、PI、HrtimPwm_SetDuty ... */
}
```

配合要点：

- 帧数取 1（`frame_count = 1`）时，一次 DMA 完成正好等于一帧，`HAL_ADC_ConvCpltCallback` 的频率等于 ADC 触发频率，半传输回调不会触发；
- 控制回调里只做采样快照、保护判断、PI 计算和占空比写入，不做格式化打印和阻塞调用；
- 保护动作只用 `HrtimPwm_DisableOutputs`，保留计数器（见 6.6 节）；
- 上电后先用示波器确认控制回调的实际频率（可在回调里翻转一个空闲 GPIO）。若 ADC 触发在 Up-Down 模式下每周期产生两次，控制周期就是开关周期的一半，`Ki` 必须按实际控制周期整定。

---

## 8. UART Debug 模块对接

### 8.1 CubeMX 配置要点

- USART1 或其它 UART，波特率 115200；
- TX 开启；
- NVIC 中使能 UART 全局中断。

### 8.2 定义缓冲区

```c
#define UART_TX_SIZE 256U

static uint8_t g_uart_tx[UART_TX_SIZE];
static UartDebug g_uart;
```

### 8.3 初始化

```c
UartDebug_Init(&g_uart, &huart1, g_uart_tx, UART_TX_SIZE);
```

### 8.4 挂接发送完成回调

```c
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        UartDebug_TxCpltCallback(&g_uart);
    }
}
```

### 8.5 主循环处理与打印

```c
while (1)
{
    UartDebug_Process(&g_uart);

    static uint32_t last_print = 0U;
    if ((HAL_GetTick() - last_print) >= 50U)
    {
        last_print = HAL_GetTick();
        UartDebug_Printf(&g_uart, "vout=%.3f iout=%.3f\r\n",
                         (double)g_vout, (double)g_iout);
    }
}
```

注意：

- 不要在控制中断中调用 `UartDebug_Printf`；
- 打印浮点数时，Keil / ARM GCC 需要开启 float printf 支持；
- 调试代码空间紧张时，优先使用二进制遥测。

---

## 9. OLED 模块对接

### 9.1 CubeMX 配置要点

- 模式选 I2C，引脚配置为 AF 开漏并接外部上拉；
- `hi2c1.Init.Timing` 必须与 I2C 内核时钟匹配（见 1.1 节第 2 条）；
- 模块使用阻塞式收发，不需要使能 I2C 的 NVIC 中断。

### 9.2 接入

```c
static Oled g_oled;

Oled_Init(&g_oled, &hi2c1, OLED_ADDR_DEFAULT);   /* 0x3C << 1 = 0x78 */
```

### 9.3 显示

坐标按页组织，8x16 字库一个字符占两页，所以 `page` 取 0 / 2 / 4 / 6：

```c
Oled_ShowString(&g_oled, 0U, 0U, "4SW Buck-Boost");
Oled_ShowString(&g_oled, 0U, 2U, "Vo");
Oled_ShowFloat(&g_oled, 24U, 2U, vout, 5U, 1U);
```

要点：

- `x` 是像素列（0~127），建议按 8 递增；
- `Oled_ShowFloat` 依赖浮点 `printf`，链接时需要 `-u _printf_float`，否则小数不会显示；
- 刷屏是阻塞的 I2C 传输（从机不在线时会一直等到超时），放在主循环里按固定周期刷，不要放进控制中断；
- 模块每次都直接写屏，不做显存差分，所以只更新变化的内容即可，没必要整屏重画。

---

## 10. 四开关 Buck-Boost 工程

`apps/buck_boost_openloop/` 与 `apps/buck_boost_closeloop/` 是两个完整的 STM32CubeMX CMake 工程，均使用本库模块，模块源码以复制方式放入各自的 `Core/Src` 与 `Core/Inc`。拓扑相关的控制逻辑写在各自的 `Core/Src/main.c` 的 `USER CODE` 区，不属于本库。

### 10.1 功率级与占空比约定

```text
Vin --Q1-- SW1 --L-- SW2 --Q3-- Vout
             |           |
            Q2          Q4
             |           |
            GND         GND
```

两个桥臂都是 HRTIM 互补输出 + 死区。“占空比”统一指该桥臂**上管**的导通占空比：`dA` 是 Q1（Timer A / CMP1），`dB` 是 Q3（Timer B / CMP1）。

### 10.2 单模式控制

两个桥臂始终同占空比 `d`：

```text
vout = vin * d / (1 - d)          稳态  d = vout / (vin + vout)
```

`d > 0.5` 升压、`d < 0.5` 降压，一套 PI 覆盖全范围。控制量全程连续，不需要区间判断，也不会在边界跳变。

代价是两个桥臂一直在开关，效率低于“降压区只开 buck 桥臂”的分区方案。要提效率时按 `vin/vref` 分三个区间分别映射占空比，并给每个区间单独整定增益。

### 10.3 控制环位置与外设分工

| 任务 | 位置 |
|---|---|
| ADC 快照、保护判断、PI、占空比下发 | `HAL_ADC_ConvCpltCallback`，控制节拍由采样决定，不用 HRTIM 重复中断 |
| 串口收发、命令解析、刷屏、遥测 | `main()` 的 `while(1)` |
| 中断向量与 HAL 回调 | `HAL_ADC_ConvCpltCallback`、`HAL_UART_TxCpltCallback` 在 `main.c` 的 `USER CODE BEGIN 4`；`USART1_IRQHandler` 由 CubeMX 生成在 `stm32g4xx_it.c` |

控制回调里只做采样快照、保护判断、PI 计算和占空比写入，不做格式化打印和阻塞调用。

### 10.4 使能、软启动与保护

```text
上电: 写安全占空比 -> SyncPreload -> 起计数器 -> 立刻封波（计数器保持运行）
使能: 用稳态关系播种 PI 输出 -> 开输出 -> 给定按固定步长爬到目标
保护: 只封波、不停计数器 -> 故障消失后由控制回调自行判断恢复
```

主循环不直接改控制器状态，只写请求标志，由控制回调统一执行，避免两个上下文同时改状态。

### 10.5 上板前必须实测的两点

- 用示波器确认 TA1/TA2、TB1/TB2 的互补波形与死区，再接入功率级；
- 用一个空闲 GPIO 在控制回调里翻转，量出控制回调的实际频率。HRTIM 处于 Up-Down 模式时 CMP3 每个开关周期会被跨越两次，ADC 触发是一次还是两次取决于 `GTCMP3` 配置。据此修正控制频率常量并重新整定 `Ki`。

### 10.6 两个工程的差别

| | 反馈量来源 |
|---|---|
| `buck_boost_openloop` | `AdcSamplerChannel` 的 `scale` / `offset` 直接输出工程量 |
| `buck_boost_closeloop` | `scale=1`、`offset=0`，模块只输出低通后的原始码值；原始值到工程量由应用层按 `y = K·raw + b` 完成，`K` / `b` 为待标定系数 |

`buck_boost_closeloop` 用 `CALIBRATED` 宏控制是否允许使能：`K` / `b` 未标定时反馈量不具备物理意义，使能命令会被拒绝。标定方法与串口命令见该工程 README。

---

## 11. 中断与实时性建议

- HRTIM 重复中断中只做：ADC 快照更新、PI 计算、HRTIM 占空比更新；
- 滤波、保护、状态机如果需要高实时性，也应放在控制中断或固定周期任务中；
- 串口格式化、OLED、按键、参数解析放在主循环；
- 共享变量尽量使用 32 位以内类型，保证单次访问原子性；
- 多字节结构在中断和主循环之间传递时，使用快照或双缓冲。

---

## 12. 接入检查清单

1. CubeMX 中 HRTIM、ADC、DMA、UART、I2C 是否已正确配置；
2. 是否在 ADC 启动 DMA 前完成校准；
3. ADC Rank 顺序是否与 `AdcSamplerChannel` 数组一致；
4. DMA 是否为 Circular、Half Word；
5. HRTIM 是否使能 Preload 和需要的 Dead Time；
6. 互补输出是否依赖 Dead Time Insertion；
7. 多相启动是否使用 `HrtimPwm_StartAll`；
8. 是否先验证 PWM 波形和 ADC 快照，再闭环；
9. PI 的 `Ki` 是否按当前控制周期整定；
10. 输出限幅和最小/最大占空比是否正确；
11. 开输出之前是否调用过 `HrtimPwm_SyncPreload`；
12. 保护动作是否用 `HrtimPwm_DisableOutputs` 而不是 `StopAll`（ADC 由 HRTIM 触发时）；
13. 控制环在 ADC DMA 回调里是否用 `AdcSampler_UpdateFrame` 而不是 `AdcSampler_Update`；
14. USART 全局中断是否已使能，`USART1_IRQHandler` 是否只有一个定义；
15. I2C 的 `Timing` 是否与内核时钟匹配；
16. 用到 `%f` 时链接是否加了 `-u _printf_float`。
