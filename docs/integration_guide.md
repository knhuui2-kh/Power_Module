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

## 2. 构建接入

在目标工程 CMake 中：

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
- 使用 HRTIM、ADC、UART 时，再加入 `POWER_DRIVER_SOURCES`。

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

## 9. 中断与实时性建议

- HRTIM 重复中断中只做：ADC 快照更新、PI 计算、HRTIM 占空比更新；
- 滤波、保护、状态机如果需要高实时性，也应放在控制中断或固定周期任务中；
- 串口格式化、OLED、按键、参数解析放在主循环；
- 共享变量尽量使用 32 位以内类型，保证单次访问原子性；
- 多字节结构在中断和主循环之间传递时，使用快照或双缓冲。

---

## 10. 接入检查清单

1. CubeMX 中 HRTIM、ADC、DMA、UART 是否已正确配置；
2. 是否在 ADC 启动 DMA 前完成校准；
3. ADC Rank 顺序是否与 `AdcSamplerChannel` 数组一致；
4. DMA 是否为 Circular、Half Word；
5. HRTIM 是否使能 Preload 和需要的 Dead Time；
6. 互补输出是否依赖 Dead Time Insertion；
7. 多相启动是否使用 `HrtimPwm_StartAll`；
8. 是否先验证 PWM 波形和 ADC 快照，再闭环；
9. PI 的 `Ki` 是否按当前控制周期整定；
10. 输出限幅和最小/最大占空比是否正确。
