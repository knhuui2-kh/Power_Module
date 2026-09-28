# module_power

`module_power` 是面向电赛电源方向的 STM32G4 HAL 模块库，聚焦可复用的控制算法、外设适配与通用控制模式。仓库仅维护核心源码、接入文档和构建清单；具体赛题应用、验证工程和调试代码建议在独立工程中维护。

## 适用范围

- 目标平台：STM32G4 + STM32CubeMX + HAL
- 外设模块：HRTIM、ADC + DMA、UART
- 控制模块：PID、滤波器、通用单闭环控制模式
- 不包含：完整赛题应用、板级 BSP、CubeMX 外设初始化代码、临时验证工程

## 模块组成

| 模块 | 目录 | 依赖 HAL | 职责 |
|---|---|---|---|
| PID | `modules/algorithm/pid` | 否 | 位置式 PID、测量值微分、条件积分抗饱和 |
| Filter | `modules/algorithm/filter` | 否 | 一阶低通、滑动平均、中值滤波 |
| HRTIM PWM | `modules/driver/hrtim_pwm` | 是 | HRTIM 逻辑通道、占空比、启停、同步启动 |
| ADC Sampler | `modules/driver/adc_sampler` | 是 | HRTIM 触发 + DMA 循环采样、标定、快照 |
| UART Debug | `modules/driver/uart_debug` | 是 | 非阻塞环形缓冲、格式化打印、丢包统计 |
| PowerLoop | `modules/pattern/power_loop` | 间接 | 组合 ADC、PID、HRTIM 的通用单闭环模式 |

## 架构与依赖

```text
pattern  ---> algorithm
   |
   +-------> driver ---> HAL / CubeMX
```

- `modules/algorithm`：纯 C 算法层，不依赖 HAL，便于移植和复用。
- `modules/driver`：硬件语义适配层，直接调用 HAL；不包含控制策略。
- `modules/pattern`：通用控制模式层，组合算法层和驱动层；不直接操作寄存器，也不包含赛题业务状态机。

## 快速接入

1. 按 `docs/g474_cube_integration.md` 完成 CubeMX 配置。
2. 在目标工程的 CMake 配置中 `include` 根目录下的 `power_modules.cmake`，按需加入 `POWER_ALGORITHM_SOURCES`、`POWER_DRIVER_SOURCES`、`POWER_PATTERN_SOURCES` 和 `POWER_INCLUDE_DIRS`。
3. 单闭环可直接使用 `PowerLoop`；多环、SPWM/SVPWM 等算法建议逐步沉淀到 `modules/algorithm`。
4. `PowerLoop` 接口见 `modules/pattern/power_loop.h`；已有 SPWM/SVPWM 的接入方式见 `docs/g474_cube_integration.md`。

## 设计约定

- 不使用动态内存，存储区由调用方提供。
- 初始化与周期更新分离；高频路径中不调用阻塞 HAL、不做字符串格式化。
- `min_duty`、`max_duty`、输出限幅必须显式配置。
- CubeMX 生成区不手工修改；用户代码放在 `USER CODE` 区域。
- 公共头文件采用模块前缀命名，如 `power_pid.h`、`hrtim_pwm.h`。
- 模块保持单一职责：新增职责前先判断是否应新建模块，而不是继续扩大既有模块。

## 维护规则

- 仓库只保留核心模块、接入文档和构建清单。
- 验证工程、单元测试、样例工程和调参脚本在库外独立维护，验证结论再合并回核心代码。
- 修改驱动层时序后，必须验证采样点、死区、相位和封波顺序。
- 不提交 `build/`、`Debug/`、`Release/`、`.o`、`.elf`、`.hex` 等构建产物。
- 删除功能时同步清理源文件、头文件、构建清单和文档引用，避免遗留无效内容。

## 自动化与 AI 辅助开发

- 算法层无 HAL 依赖，接口稳定，适合自动化修改和外部验证。
- 驱动层 API 与数据结构名属于稳定契约；内部实现可优化，公开语义不应随意变更。
- 修改控制逻辑前请先阅读 `docs/architecture.md`，明确修改范围和硬件验证要求。
