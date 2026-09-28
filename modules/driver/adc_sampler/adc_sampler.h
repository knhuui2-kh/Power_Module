#ifndef POWER_ADC_SAMPLER_H
#define POWER_ADC_SAMPLER_H

#include "stm32g4xx_hal.h"
#include "power_filter.h"

#define ADC_SAMPLER_MAX_CHANNELS 8U

/*
 * DMA 循环缓冲区布局：一帧包含 channel_count 个 ADC 值，顺序与 CubeMX Rank 一致。
 * 每个逻辑通道独立配置标定和低通，更换硬件时通常只改这里。
 */
typedef struct
{
    float scale;
    float offset;
    float lpf_alpha;
    Lpf1 lpf;
} AdcSamplerChannel;

typedef struct
{
    ADC_HandleTypeDef *hadc;
    uint16_t *dma_buf;
    uint16_t frame_count;
    uint16_t channel_count;
    AdcSamplerChannel ch[ADC_SAMPLER_MAX_CHANNELS];
    uint16_t raw[ADC_SAMPLER_MAX_CHANNELS];
    float value[ADC_SAMPLER_MAX_CHANNELS];
    uint32_t last_frame;
    uint8_t has_data;
    uint8_t updated;
    uint8_t started;
} AdcSampler;

HAL_StatusTypeDef AdcSampler_Init(AdcSampler *sampler, ADC_HandleTypeDef *hadc,
                                  uint16_t *dma_buf, uint16_t frame_count,
                                  uint16_t channel_count,
                                  const AdcSamplerChannel *channels);
HAL_StatusTypeDef AdcSampler_Start(AdcSampler *sampler);
HAL_StatusTypeDef AdcSampler_Stop(AdcSampler *sampler);
void AdcSampler_Update(AdcSampler *sampler);
float AdcSampler_Get(const AdcSampler *sampler, uint16_t channel);
uint16_t AdcSampler_GetRaw(const AdcSampler *sampler, uint16_t channel);
uint8_t AdcSampler_HasData(const AdcSampler *sampler);
uint8_t AdcSampler_IsUpdated(const AdcSampler *sampler);

#endif
