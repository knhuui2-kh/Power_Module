# STM32G474 + CubeMX 接入指南

## 1. CubeMX 关键配置

### HRTIM1

- 使用内部或外部时钟，完成 DLL 校准。
- 以 TIMER_A 为三相/单相主定时器，TIMER_B、TIMER_C 可同步启动。
- Counting Mode：Up；Prescaler 按开关频率选择；`Period = round(fHRCK / fsw) - 1`。
- Preload：Enabled；Repetition Update：Enabled，确保占空比在周期边界更新。
- Deadtime Insertion：需要互补输出时使能，死区值按驱动和 MOS 规格计算。
- 输出：TA1/TA2、TB1/TB2、TC1/TC2 配置为 AF13。
- ADC Trigger：需要同步采样时，用 TIMER_A PERIOD 事件触发 `HRTIM_TRG1`。

### ADC1 + DMA

- Scan Mode：Enabled，Rank 顺序与 `AdcSamplerChannel` 数组一一对应。
- External Trigger：`ADC_EXTERNALTRIG_HRTIM_TRG1`，上升沿。
- Continuous DMA Requests：Enabled；Overrun：Preserved。
- DMA：Circular、Peripheral to Memory、Memory Increment、Half Word。
- ADC 校准在启动 DMA 前调用：

```c
HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
```

### USART

- 波特率 115200，8N1，开启 TX 和 NVIC。
- 建议 UART 专用于调试遥测，不要在中断中调用阻塞发送。

## 2. 工程接入

- 添加源文件目录：
  - `modules/algorithm/pid`
  - `modules/algorithm/filter`
  - `modules/driver/hrtim_pwm`
  - `modules/driver/adc_sampler`
  - `modules/driver/uart_debug`
  - `modules/pattern`
- 添加对应 include 路径。
- CubeMX 重新生成时只保留 `USER CODE` 区域中的调用。

## 3. 用户代码挂接点

- `main()` 初始化 CubeMX 外设后，调用应用初始化。
- `HAL_HRTIM_RepetitionEventCallback()` 中调用控制更新，避免耗时打印。
- `HAL_UART_TxCpltCallback()` 中调用 `UartDebug_TxCpltCallback()`。
- 主循环调用 `UartDebug_Process()` 和低频遥测打印。

## 4. 时序建议

- 三相或多相应用使用 `HrtimPwm_StartAll()` 统一启动，避免逐个启动带来相位差。
- 控制频率 = HRTIM 中断频率；PID 的 `dt` 取该周期，例如 20 kHz 对应 50 us。
- `AdcSampler_Update()` 放在控制中断开头，使用最近一帧完整数据；`AdcSampler_IsUpdated()` 可用于采样超时保护。
- `HrtimPwm_SetDuty()` 写入后由 HRTIM 预装载在周期边界生效。
- 遥测频率控制在 10~100 Hz，避免串口占用控制时间。

## 5. 注意事项

- `UartDebug_Printf` 使用 `vsnprintf`，打印 float 时 Keil/ARM GCC 可能需要开启 float printf 选项；调试代码空间紧张时改用整数或二进制遥测。
- `AdcSampler_Update` 假设 DMA 为循环模式，并且一个帧的长度等于 `channel_count`；改 Rank 顺序或通道数时必须同步修改配置。
- `HrtimPwm` 的标准互补输出依赖 CubeMX 中已使能 Dead Time Insertion；若 `output2` 配置为 0，则只启动单端输出。
- 关闭输出优先调用 `HrtimPwm_StopAll()`；它会把占空比恢复到 `min_duty` 并关闭输出和计数器。
- 采样、控制、保护参数先小范围验证，再接入完整功率级。

## 6. 接入已有 SPWM/SVPWM 代码

参考工程里的 `spwm.c`、`svpwm.c` 已验证，核心输出就是三路归一化占空比
`Ta`、`Tb`、`Tc`。接入本模块时不需要改算法本身：

```c
/* 控制中断中，在完成 SPWM/SVPWM 计算后执行。 */
(void)HrtimPwm_SetDuty(&s_pwm, 0U, foc.Ta);
(void)HrtimPwm_SetDuty(&s_pwm, 1U, foc.Tb);
(void)HrtimPwm_SetDuty(&s_pwm, 2U, foc.Tc);
```

如果后续想把 `spwm/svpwm` 放进 `modules/algorithm`，只需去掉其中的 HAL 依赖，
把输入输出保持为归一化浮点量，底层写比较值的动作继续由 `HrtimPwm` 负责。

## 7. CMake 接入示例

```cmake
include(${CMAKE_SOURCE_DIR}/module_power/power_modules.cmake)

target_sources(your_target PRIVATE
    ${POWER_ALGORITHM_SOURCES}
    ${POWER_DRIVER_SOURCES}
    ${POWER_PATTERN_SOURCES}
)

target_include_directories(your_target PRIVATE
    ${POWER_INCLUDE_DIRS}
)
```
