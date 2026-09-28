#ifndef POWER_LOOP_H
#define POWER_LOOP_H

#include "adc_sampler.h"
#include "hrtim_pwm.h"
#include "power_pid.h"

#include <stdint.h>

/* 通用单闭环模式层：组合 ADC / PID / HRTIM，不直接操作 HAL。 */
typedef struct
{
    Pid pid;
    AdcSampler *adc;
    HrtimPwm *pwm;
    uint16_t adc_channel;
    uint8_t pwm_channel;
    float reference;
    float feedback;
    float output;
    uint8_t enabled;
    uint8_t initialized;
} PowerLoop;

void PowerLoop_Init(PowerLoop *loop, AdcSampler *adc, uint16_t adc_channel,
                    HrtimPwm *pwm, uint8_t pwm_channel);
void PowerLoop_ConfigPid(PowerLoop *loop, float kp, float ki, float kd,
                         float dt, float out_min, float out_max);
void PowerLoop_SetReference(PowerLoop *loop, float reference);
void PowerLoop_Enable(PowerLoop *loop);
void PowerLoop_Disable(PowerLoop *loop);
void PowerLoop_Update(PowerLoop *loop);
float PowerLoop_GetFeedback(const PowerLoop *loop);
float PowerLoop_GetOutput(const PowerLoop *loop);

#endif
