# 在 CubeMX/VSCode 的 CMake 工程中 include 本文件即可获得模块源文件和头文件路径。
set(POWER_MODULE_ROOT ${CMAKE_CURRENT_LIST_DIR})

set(POWER_ALG_SOURCES
    ${POWER_MODULE_ROOT}/modules/alg/filter/power_filter.c
    ${POWER_MODULE_ROOT}/modules/alg/pid/power_pid.c
)

set(POWER_IO_SOURCES
    ${POWER_MODULE_ROOT}/modules/io/adc_sampler/adc_sampler.c
    ${POWER_MODULE_ROOT}/modules/io/hrtim_pwm/hrtim_pwm.c
    ${POWER_MODULE_ROOT}/modules/io/uart_debug/uart_debug.c
)

set(POWER_APP_SOURCES
    ${POWER_MODULE_ROOT}/modules/app/power_loop.c
)

set(POWER_INCLUDE_DIRS
    ${POWER_MODULE_ROOT}/modules/alg/filter
    ${POWER_MODULE_ROOT}/modules/alg/pid
    ${POWER_MODULE_ROOT}/modules/io/adc_sampler
    ${POWER_MODULE_ROOT}/modules/io/hrtim_pwm
    ${POWER_MODULE_ROOT}/modules/io/uart_debug
    ${POWER_MODULE_ROOT}/modules/app
)
