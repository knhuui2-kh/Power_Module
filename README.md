# module_power

`module_power` 是面向电赛电源方向的 STM32G4 HAL 核心模块库，主要提供可复用的控制算法与外设驱动适配模块。

仓库只维护核心源码、模块文档和构建清单。CubeMX 生成代码、具体赛题应用、板级 BSP、验证工程和调试脚本由外部工程负责，不进入本仓库。

## 适用范围

- 目标平台：STM32G4 + STM32CubeMX + HAL
- 算法模块：增量式 PI、滤波器
- 驱动模块：HRTIM PWM、ADC + DMA 采样、UART 调试输出
- 不包含：完整固件、`main.c`、CubeMX 外设初始化、板级引脚定义、临时验证工程

## 目录结构

```text
module_power/
├─ modules/
│  ├─ algorithm/
│  │  ├─ pid/
│  │  │  ├─ power_pid.h
│  │  │  └─ power_pid.c
│  │  └─ filter/
│  │     ├─ power_filter.h
│  │     └─ power_filter.c
│  └─ driver/
│     ├─ hrtim_pwm/
│     │  ├─ hrtim_pwm.h
│     │  └─ hrtim_pwm.c
│     ├─ adc_sampler/
│     │  ├─ adc_sampler.h
│     │  └─ adc_sampler.c
│     └─ uart_debug/
│        ├─ uart_debug.h
│        └─ uart_debug.c
├─ docs/
│  ├─ api_reference.md
│  └─ integration_guide.md
├─ power_modules.cmake
├─ README.md
└─ .gitignore
```

## 模块组成

| 模块 | 目录 | 依赖 HAL | 职责 |
|---|---|---|---|
| PID | `modules/algorithm/pid` | 否 | 增量式 PI、固定控制周期、输出限幅 |
| Filter | `modules/algorithm/filter` | 否 | 一阶低通、滑动平均、中值滤波 |
| HRTIM PWM | `modules/driver/hrtim_pwm` | 是 | HRTIM 逻辑通道、占空比、启停、同步启动 |
| ADC Sampler | `modules/driver/adc_sampler` | 是 | HRTIM 触发 + DMA 循环采样、标定、快照 |
| UART Debug | `modules/driver/uart_debug` | 是 | 非阻塞环形缓冲、格式化打印、丢包统计 |

## 架构与数据流

```text
modules/algorithm   纯 C，无 HAL
        ^
        |
modules/driver      硬件语义适配，直接调用 HAL
        ^
        |
CubeMX / 目标工程    时钟、GPIO、外设初始化、句柄
```

典型控制链路：

```text
HRTIM 周期事件
   -> ADC 触发
   -> DMA 搬运
   -> AdcSampler_Update
   -> 工程量快照
   -> PI / 滤波
   -> HrtimPwm_SetDuty
   -> HRTIM 比较值
   -> 功率级
```

算法层可以脱离硬件独立编译；驱动层依赖 STM32G4 HAL，但通过句柄注入与具体 CubeMX 工程解耦。

## 快速接入

1. 按 `docs/integration_guide.md` 完成 CubeMX 配置。
2. 在目标工程 CMake 中 `include` 根目录下的 `power_modules.cmake`，按需加入 `POWER_ALGORITHM_SOURCES`、`POWER_DRIVER_SOURCES` 和 `POWER_INCLUDE_DIRS`。
3. 将 CubeMX 生成的句柄传入驱动模块，例如 `&hhrtim1`、`&hadc1`、`&huart1`。
4. 具体调用位置、缓冲区定义和回调挂接见 `docs/integration_guide.md`。

## 设计约定

- 不使用动态内存，缓冲区由调用方提供。
- 初始化与周期更新分离；高频路径中不调用阻塞 HAL、不做字符串格式化。
- `min_duty`、`max_duty`、输出限幅必须显式配置。
- CubeMX 生成区不手工修改，用户代码放在 `USER CODE` 区域。
- 公共头文件采用模块前缀命名，如 `power_pid.h`、`hrtim_pwm.h`。
- 模块保持单一职责，新增职责前先判断是否应新建模块。
- PID 为增量式 PI，`Ki` 为每周期积分系数；改变控制频率后需要重新整定。

## 文档

- `docs/api_reference.md`：各模块 API 速查与实现说明。
- `docs/integration_guide.md`：各模块如何对接 CubeMX 及目标工程。

## 维护规则

- 仓库只保留核心模块、模块文档和构建清单。
- 验证工程、单元测试、样例工程和调参脚本在库外独立维护，验证结论再合并回核心代码。
- 修改驱动层时序后，必须验证采样点、死区、相位和封波顺序。
- 不提交 `build/`、`Debug/`、`Release/`、`.o`、`.elf`、`.hex` 等构建产物。
- 删除功能时同步清理源文件、头文件、构建清单和文档引用，避免遗留无效内容。
