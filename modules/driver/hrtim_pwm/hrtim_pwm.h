#ifndef POWER_HRTIM_PWM_H
#define POWER_HRTIM_PWM_H

#include "stm32g4xx_hal.h"

#define HRTIM_PWM_MAX_CHANNELS 6U

/*
 * 一个逻辑通道对应一个 HRTIM Timer。
 * output1 必须配置；output2 为 0 表示单端输出。
 * configure_waveform = 1 时，模块写入 CMP1 和标准边沿输出配置。
 */
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

typedef struct
{
    HRTIM_HandleTypeDef *hhrtim;
    uint8_t channel_count;
    HrtimPwmChannelCfg cfg[HRTIM_PWM_MAX_CHANNELS];
    uint32_t period[HRTIM_PWM_MAX_CHANNELS];
    float duty[HRTIM_PWM_MAX_CHANNELS];
    uint8_t started[HRTIM_PWM_MAX_CHANNELS];
    uint8_t ready;
} HrtimPwm;

HAL_StatusTypeDef HrtimPwm_Init(HrtimPwm *pwm, HRTIM_HandleTypeDef *hhrtim,
                                const HrtimPwmChannelCfg *cfg, uint8_t channel_count);
HAL_StatusTypeDef HrtimPwm_Start(HrtimPwm *pwm, uint8_t channel);
HAL_StatusTypeDef HrtimPwm_StartAll(HrtimPwm *pwm);
HAL_StatusTypeDef HrtimPwm_Stop(HrtimPwm *pwm, uint8_t channel);
void HrtimPwm_StopAll(HrtimPwm *pwm);
HAL_StatusTypeDef HrtimPwm_SetDuty(HrtimPwm *pwm, uint8_t channel, float duty);
float HrtimPwm_GetDuty(const HrtimPwm *pwm, uint8_t channel);
HAL_StatusTypeDef HrtimPwm_SetPeriod(HrtimPwm *pwm, uint8_t channel, uint32_t period);
uint32_t HrtimPwm_GetPeriod(const HrtimPwm *pwm, uint8_t channel);
uint32_t HrtimPwm_DutyToCompare(const HrtimPwm *pwm, uint8_t channel, float duty);
uint32_t HrtimPwm_PeriodFromFreq(float freq_hz, float hrtim_clock_hz);

#endif
