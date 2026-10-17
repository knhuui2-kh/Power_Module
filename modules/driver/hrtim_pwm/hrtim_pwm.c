#include "hrtim_pwm.h"

#include <stddef.h>
#include <string.h>

static float HrtimPwm_Clamp(float value, float min, float max)
{
    if (value > max)
    {
        return max;
    }

    if (value < min)
    {
        return min;
    }

    return value;
}

static uint32_t HrtimPwm_CalcCompare(uint32_t period, float duty,
                                     float min_duty, float max_duty)
{
    uint32_t compare;

    if (period < 2U)
    {
        return 0U;
    }

    duty = HrtimPwm_Clamp(duty, min_duty, max_duty);
    compare = (uint32_t)((float)period * duty + 0.5f);

    if (compare < 1U)
    {
        compare = 1U;
    }

    if (compare >= period)
    {
        compare = period - 1U;
    }

    return compare;
}

HAL_StatusTypeDef HrtimPwm_Init(HrtimPwm *pwm, HRTIM_HandleTypeDef *hhrtim,
                                const HrtimPwmChannelCfg *cfg, uint8_t channel_count)
{
    uint8_t i;

    if ((pwm == NULL) || (hhrtim == NULL) || (cfg == NULL)
        || (channel_count == 0U) || (channel_count > HRTIM_PWM_MAX_CHANNELS))
    {
        return HAL_ERROR;
    }

    memset(pwm, 0, sizeof(*pwm));
    pwm->hhrtim = hhrtim;
    pwm->channel_count = channel_count;

    for (i = 0U; i < channel_count; i++)
    {
        HRTIM_CompareCfgTypeDef compare_cfg = {0};
        HRTIM_OutputCfgTypeDef output_cfg = {0};

        if ((cfg[i].output1 == 0U)
            || (cfg[i].timer_index > HRTIM_TIMERINDEX_TIMER_F)
            || (cfg[i].min_duty < 0.0f)
            || (cfg[i].max_duty > 1.0f)
            || (cfg[i].min_duty > cfg[i].max_duty))
        {
            return HAL_ERROR;
        }

        pwm->cfg[i] = cfg[i];
        pwm->period[i] = __HAL_HRTIM_GETPERIOD(hhrtim, cfg[i].timer_index);

        if (pwm->period[i] < 2U)
        {
            return HAL_ERROR;
        }

        pwm->duty[i] = HrtimPwm_Clamp(cfg[i].init_duty,
                                      cfg[i].min_duty, cfg[i].max_duty);

        __HAL_HRTIM_SETCOMPARE(hhrtim, cfg[i].timer_index, HRTIM_COMPAREUNIT_1,
                               HrtimPwm_CalcCompare(pwm->period[i],
                                                    pwm->duty[i],
                                                    cfg[i].min_duty,
                                                    cfg[i].max_duty));

        if (cfg[i].configure_waveform != 0U)
        {
            compare_cfg.CompareValue = HrtimPwm_CalcCompare(pwm->period[i],
                                                            pwm->duty[i],
                                                            cfg[i].min_duty,
                                                            cfg[i].max_duty);
            compare_cfg.AutoDelayedMode = HRTIM_AUTODELAYEDMODE_REGULAR;
            compare_cfg.AutoDelayedTimeout = 0U;

            if (HAL_HRTIM_WaveformCompareConfig(hhrtim, cfg[i].timer_index,
                                                HRTIM_COMPAREUNIT_1,
                                                &compare_cfg) != HAL_OK)
            {
                return HAL_ERROR;
            }

            output_cfg.Polarity = HRTIM_OUTPUTPOLARITY_HIGH;
            output_cfg.SetSource = HRTIM_OUTPUTSET_TIMPER;
            output_cfg.ResetSource = HRTIM_OUTPUTRESET_TIMCMP1;
            output_cfg.IdleMode = HRTIM_OUTPUTIDLEMODE_NONE;
            output_cfg.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
            output_cfg.FaultLevel = HRTIM_OUTPUTFAULTLEVEL_NONE;
            output_cfg.ChopperModeEnable = HRTIM_OUTPUTCHOPPERMODE_DISABLED;
            output_cfg.BurstModeEntryDelayed = HRTIM_OUTPUTBURSTMODEENTRY_REGULAR;

            if (HAL_HRTIM_WaveformOutputConfig(hhrtim, cfg[i].timer_index,
                                               cfg[i].output1,
                                               &output_cfg) != HAL_OK)
            {
                return HAL_ERROR;
            }

            if (cfg[i].output2 != 0U)
            {
                /* 互补输出交给 HRTIM 死区逻辑，CubeMX 中需使能 DeadTimeInsertion。 */
                output_cfg.SetSource = HRTIM_OUTPUTSET_NONE;
                output_cfg.ResetSource = HRTIM_OUTPUTRESET_NONE;

                if (HAL_HRTIM_WaveformOutputConfig(hhrtim, cfg[i].timer_index,
                                                   cfg[i].output2,
                                                   &output_cfg) != HAL_OK)
                {
                    return HAL_ERROR;
                }
            }
        }

        (void)HAL_HRTIM_WaveformOutputStop(hhrtim, cfg[i].output1 | cfg[i].output2);
    }

    pwm->ready = 1U;
    return HAL_OK;
}

HAL_StatusTypeDef HrtimPwm_Start(HrtimPwm *pwm, uint8_t channel)
{
    HAL_StatusTypeDef status;
    uint32_t output_mask;

    if ((pwm == NULL) || (pwm->ready == 0U) || (channel >= pwm->channel_count))
    {
        return HAL_ERROR;
    }

    if (pwm->started[channel] != 0U)
    {
        return HAL_OK;
    }

    output_mask = pwm->cfg[channel].output1 | pwm->cfg[channel].output2;
    status = HAL_HRTIM_WaveformOutputStart(pwm->hhrtim, output_mask);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_HRTIM_WaveformCountStart(pwm->hhrtim, pwm->cfg[channel].timer_id);
    if (status != HAL_OK)
    {
        (void)HAL_HRTIM_WaveformOutputStop(pwm->hhrtim, output_mask);
        return status;
    }

    pwm->started[channel] = 1U;
    return HAL_OK;
}

HAL_StatusTypeDef HrtimPwm_StartAll(HrtimPwm *pwm)
{
    HAL_StatusTypeDef status;
    uint32_t output_mask = 0U;
    uint32_t timer_mask = 0U;
    uint8_t i;

    if ((pwm == NULL) || (pwm->ready == 0U))
    {
        return HAL_ERROR;
    }

    for (i = 0U; i < pwm->channel_count; i++)
    {
        if (pwm->started[i] == 0U)
        {
            output_mask |= pwm->cfg[i].output1 | pwm->cfg[i].output2;
            timer_mask |= pwm->cfg[i].timer_id;
        }
    }

    if (timer_mask == 0U)
    {
        return HAL_OK;
    }

    status = HAL_HRTIM_WaveformOutputStart(pwm->hhrtim, output_mask);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_HRTIM_WaveformCountStart(pwm->hhrtim, timer_mask);
    if (status != HAL_OK)
    {
        (void)HAL_HRTIM_WaveformOutputStop(pwm->hhrtim, output_mask);
        return status;
    }

    for (i = 0U; i < pwm->channel_count; i++)
    {
        pwm->started[i] = 1U;
    }

    return HAL_OK;
}

HAL_StatusTypeDef HrtimPwm_Stop(HrtimPwm *pwm, uint8_t channel)
{
    HAL_StatusTypeDef status_out;
    HAL_StatusTypeDef status_cnt;
    uint32_t output_mask;

    if ((pwm == NULL) || (pwm->ready == 0U) || (channel >= pwm->channel_count))
    {
        return HAL_ERROR;
    }

    if (pwm->started[channel] == 0U)
    {
        return HAL_OK;
    }

    output_mask = pwm->cfg[channel].output1 | pwm->cfg[channel].output2;
    status_out = HAL_HRTIM_WaveformOutputStop(pwm->hhrtim, output_mask);
    status_cnt = HAL_HRTIM_WaveformCountStop(pwm->hhrtim, pwm->cfg[channel].timer_id);
    pwm->started[channel] = 0U;

    return ((status_out == HAL_OK) && (status_cnt == HAL_OK)) ? HAL_OK : HAL_ERROR;
}

void HrtimPwm_StopAll(HrtimPwm *pwm)
{
    uint8_t i;

    if ((pwm == NULL) || (pwm->ready == 0U))
    {
        return;
    }

    for (i = 0U; i < pwm->channel_count; i++)
    {
        (void)HrtimPwm_Stop(pwm, i);
        (void)HrtimPwm_SetDuty(pwm, i, pwm->cfg[i].min_duty);
    }
}

/* 只开关输出，不碰计数器——ADC 由 HRTIM 触发时必须用这一对。 */
HAL_StatusTypeDef HrtimPwm_EnableOutputs(HrtimPwm *pwm)
{
    uint32_t output_mask = 0U;
    uint8_t i;

    if ((pwm == NULL) || (pwm->ready == 0U))
    {
        return HAL_ERROR;
    }

    for (i = 0U; i < pwm->channel_count; i++)
    {
        output_mask |= pwm->cfg[i].output1 | pwm->cfg[i].output2;
    }

    return HAL_HRTIM_WaveformOutputStart(pwm->hhrtim, output_mask);
}

HAL_StatusTypeDef HrtimPwm_DisableOutputs(HrtimPwm *pwm)
{
    uint32_t output_mask = 0U;
    uint8_t i;

    if ((pwm == NULL) || (pwm->ready == 0U))
    {
        return HAL_ERROR;
    }

    for (i = 0U; i < pwm->channel_count; i++)
    {
        output_mask |= pwm->cfg[i].output1 | pwm->cfg[i].output2;
    }

    return HAL_HRTIM_WaveformOutputStop(pwm->hhrtim, output_mask);
}

/* 强制一次软件更新，把预装载寄存器搬进有效寄存器。开输出前调用。 */
HAL_StatusTypeDef HrtimPwm_SyncPreload(HrtimPwm *pwm)
{
    uint32_t timer_mask = 0U;
    uint8_t i;

    if ((pwm == NULL) || (pwm->ready == 0U))
    {
        return HAL_ERROR;
    }

    for (i = 0U; i < pwm->channel_count; i++)
    {
        timer_mask |= pwm->cfg[i].timer_id;
    }

    return HAL_HRTIM_SoftwareUpdate(pwm->hhrtim, timer_mask);
}

HAL_StatusTypeDef HrtimPwm_SetDuty(HrtimPwm *pwm, uint8_t channel, float duty)
{
    uint32_t compare;

    if ((pwm == NULL) || (pwm->ready == 0U) || (channel >= pwm->channel_count))
    {
        return HAL_ERROR;
    }

    pwm->duty[channel] = HrtimPwm_Clamp(duty,
                                        pwm->cfg[channel].min_duty,
                                        pwm->cfg[channel].max_duty);
    compare = HrtimPwm_CalcCompare(pwm->period[channel],
                                   pwm->duty[channel],
                                   pwm->cfg[channel].min_duty,
                                   pwm->cfg[channel].max_duty);

    __HAL_HRTIM_SETCOMPARE(pwm->hhrtim, pwm->cfg[channel].timer_index,
                           HRTIM_COMPAREUNIT_1, compare);
    return HAL_OK;
}

float HrtimPwm_GetDuty(const HrtimPwm *pwm, uint8_t channel)
{
    if ((pwm == NULL) || (channel >= pwm->channel_count))
    {
        return 0.0f;
    }

    return pwm->duty[channel];
}

HAL_StatusTypeDef HrtimPwm_SetPeriod(HrtimPwm *pwm, uint8_t channel, uint32_t period)
{
    uint32_t compare;

    if ((pwm == NULL) || (pwm->ready == 0U)
        || (channel >= pwm->channel_count) || (period < 2U))
    {
        return HAL_ERROR;
    }

    pwm->period[channel] = period;
    compare = HrtimPwm_CalcCompare(period,
                                   pwm->duty[channel],
                                   pwm->cfg[channel].min_duty,
                                   pwm->cfg[channel].max_duty);

    __HAL_HRTIM_SETPERIOD(pwm->hhrtim, pwm->cfg[channel].timer_index, period);
    __HAL_HRTIM_SETCOMPARE(pwm->hhrtim, pwm->cfg[channel].timer_index,
                           HRTIM_COMPAREUNIT_1, compare);
    return HAL_OK;
}

uint32_t HrtimPwm_GetPeriod(const HrtimPwm *pwm, uint8_t channel)
{
    if ((pwm == NULL) || (channel >= pwm->channel_count))
    {
        return 0U;
    }

    return pwm->period[channel];
}

uint32_t HrtimPwm_DutyToCompare(const HrtimPwm *pwm, uint8_t channel, float duty)
{
    if ((pwm == NULL) || (channel >= pwm->channel_count))
    {
        return 0U;
    }

    return HrtimPwm_CalcCompare(pwm->period[channel], duty,
                                pwm->cfg[channel].min_duty,
                                pwm->cfg[channel].max_duty);
}

uint32_t HrtimPwm_PeriodFromFreq(float freq_hz, float hrtim_clock_hz)
{
    float ticks;
    uint32_t period;

    if ((freq_hz <= 0.0f) || (hrtim_clock_hz <= 0.0f))
    {
        return 0U;
    }

    ticks = hrtim_clock_hz / freq_hz;
    if (ticks < 2.0f)
    {
        ticks = 2.0f;
    }

    period = (uint32_t)(ticks + 0.5f) - 1U;
    if (period < 1U)
    {
        period = 1U;
    }

    if (period > 0xFFFFU)
    {
        period = 0xFFFFU;
    }

    return period;
}
