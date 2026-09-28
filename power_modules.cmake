# CMake 构建清单：在目标工程中 include 本文件，获取模块源码与头文件路径。
set(POWER_MODULE_ROOT ${CMAKE_CURRENT_LIST_DIR})

set(POWER_ALGORITHM_SOURCES
    ${POWER_MODULE_ROOT}/modules/algorithm/filter/power_filter.c
    ${POWER_MODULE_ROOT}/modules/algorithm/pid/power_pid.c
)

set(POWER_DRIVER_SOURCES
    ${POWER_MODULE_ROOT}/modules/driver/adc_sampler/adc_sampler.c
    ${POWER_MODULE_ROOT}/modules/driver/hrtim_pwm/hrtim_pwm.c
    ${POWER_MODULE_ROOT}/modules/driver/uart_debug/uart_debug.c
)

set(POWER_PATTERN_SOURCES
    ${POWER_MODULE_ROOT}/modules/pattern/power_loop.c
)

set(POWER_INCLUDE_DIRS
    ${POWER_MODULE_ROOT}/modules/algorithm/filter
    ${POWER_MODULE_ROOT}/modules/algorithm/pid
    ${POWER_MODULE_ROOT}/modules/driver/adc_sampler
    ${POWER_MODULE_ROOT}/modules/driver/hrtim_pwm
    ${POWER_MODULE_ROOT}/modules/driver/uart_debug
    ${POWER_MODULE_ROOT}/modules/pattern
)
