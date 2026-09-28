# module_power

面向电赛电源方向的 STM32G4 HAL 核心模块库。只保留可复用源码、接入文档和构建清单；
临时验证工程、测试样例、调试脚本不进入本目录，避免长期演变成“什么都往里放”的屎山。

## 模块划分

| 模块 | 目录 | 是否依赖 HAL | 职责 |
|---|---|---|---|
| PID | `modules/alg/pid` | 否 | 位置式 PID、测量值微分、条件积分抗饱和 |
| Filter | `modules/alg/filter` | 否 | 一阶低通、滑动平均、中值滤波 |
| HRTIM PWM | `modules/io/hrtim_pwm` | 是 | HRTIM 逻辑通道、占空比、启停、同步启动 |
| ADC Sampler | `modules/io/adc_sampler` | 是 | HRTIM 触发 + DMA 循环采样、标定、快照 |
| UART Debug | `modules/io/uart_debug` | 是 | 非阻塞环形缓冲、格式化打印、丢包统计 |
| Power Loop | `modules/app` | 间接 | 组合 ADC/PID/HRTIM 的单闭环模板 |

## 依赖方向

```text
app  ->  algorithm (纯 C，无 HAL)
 |          ^
 v          |
io   -------+-> HAL / CubeMX
```

- 算法层不包含 `stm32g4xx_hal.h`，负责纯计算，便于移植和长期维护。
- 外设层只做硬件语义适配，直接调用 HAL；不实现控制策略。
- 应用层负责信号映射和调用顺序，不直接操作寄存器。

## 快速接入

1. 按 `docs/g474_cube_integration.md` 配置 CubeMX。
2. 通过 `power_modules.cmake` 或 `power_modules.mk` 把源码和 include 路径加入目标工程。
3. 单闭环可直接使用 `PowerLoop`；多环、SPWM/SVPWM 等算法逐步沉淀到 `modules/alg`。
4. `PowerLoop` 接口见 `modules/app/power_loop.h`；现有 SPWM/SVPWM 的接入方式见接入文档。

## 设计约定

- 不使用动态内存，存储区由调用方提供。
- 初始化与周期更新分离；高频路径中不调用阻塞 HAL、不格式化字符串。
- `min_duty`、`max_duty`、输出限幅必须显式配置，禁止隐式默认到危险范围。
- CubeMX 生成区不手工修改；用户代码放在 `USER CODE` 区域。
- 公共头文件按模块命名，如 `power_pid.h`、`hrtim_pwm.h`，应用层只包含需要的模块。
- 模块文件保持“小而专”：新增职责前先判断是否应该新建模块，而不是继续往旧模块里堆功能。

## 维护规则

- 本目录只保留核心模块、模块文档和构建清单。
- 临时验证工程、单元测试、样例工程、调参脚本放在库外的独立目录或仓库，验证结论再反哺核心代码。
- 新增或修改外设层时序后，必须在目标工程和硬件上验证采样点、死区、相位和封波顺序。
- 禁止把 `build/`、`Debug/`、`Release/`、`.o`、`.elf` 等编译产物提交进本目录。
- 删除功能时一并删除其头文件、源文件、构建清单条目和文档引用，不留死代码。

## AI 协作

- 算法层是 AI 修改的首选区域：无 HAL、接口稳定、行为容易验证。
- 外设层 API 和数据结构名是稳定契约；AI 可以优化内部实现，但不能随意改函数语义。
- 修改控制逻辑前先读 `docs/architecture.md`，明确可改区域、禁止事项和验证要求。
