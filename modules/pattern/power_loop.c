#include "power_loop.h"

#include <stddef.h>

void PowerLoop_Init(PowerLoop *loop, AdcSampler *adc, uint16_t adc_channel,
                    HrtimPwm *pwm, uint8_t pwm_channel)
{
    if ((loop == NULL) || (adc == NULL) || (pwm == NULL))
    {
        return;
    }

    loop->adc = adc;
    loop->pwm = pwm;
    loop->adc_channel = adc_channel;
    loop->pwm_channel = pwm_channel;
    loop->reference = 0.0f;
    loop->feedback = 0.0f;
    loop->output = 0.0f;
    loop->enabled = 0U;
    loop->initialized = 1U;

    Pid_Init(&loop->pid, 0.0f, 0.0f, 0.0f, 0.001f, 0.0f, 1.0f);
}

void PowerLoop_ConfigPid(PowerLoop *loop, float kp, float ki, float kd,
                         float dt, float out_min, float out_max)
{
    if ((loop == NULL) || (loop->initialized == 0U))
    {
        return;
    }

    Pid_Init(&loop->pid, kp, ki, kd, dt, out_min, out_max);
}

void PowerLoop_SetReference(PowerLoop *loop, float reference)
{
    if (loop != NULL)
    {
        loop->reference = reference;
    }
}

void PowerLoop_Enable(PowerLoop *loop)
{
    if ((loop == NULL) || (loop->initialized == 0U))
    {
        return;
    }

    Pid_Reset(&loop->pid);
    loop->enabled = 1U;
}

void PowerLoop_Disable(PowerLoop *loop)
{
    if ((loop == NULL) || (loop->initialized == 0U))
    {
        return;
    }

    loop->enabled = 0U;
    loop->output = 0.0f;

    if (loop->pwm != NULL)
    {
        (void)HrtimPwm_SetDuty(loop->pwm, loop->pwm_channel, 0.0f);
    }
}

void PowerLoop_Update(PowerLoop *loop)
{
    if ((loop == NULL) || (loop->initialized == 0U))
    {
        return;
    }

    AdcSampler_Update(loop->adc);
    loop->feedback = AdcSampler_Get(loop->adc, loop->adc_channel);

    if ((loop->enabled == 0U) || (AdcSampler_HasData(loop->adc) == 0U))
    {
        return;
    }

    loop->output = Pid_Update(&loop->pid, loop->reference, loop->feedback);
    (void)HrtimPwm_SetDuty(loop->pwm, loop->pwm_channel, loop->output);
}

float PowerLoop_GetFeedback(const PowerLoop *loop)
{
    return (loop != NULL) ? loop->feedback : 0.0f;
}

float PowerLoop_GetOutput(const PowerLoop *loop)
{
    return (loop != NULL) ? loop->output : 0.0f;
}
