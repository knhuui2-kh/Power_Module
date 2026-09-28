#include "adc_sampler.h"

#include <stddef.h>

HAL_StatusTypeDef AdcSampler_Init(AdcSampler *sampler, ADC_HandleTypeDef *hadc,
                                  uint16_t *dma_buf, uint16_t frame_count,
                                  uint16_t channel_count,
                                  const AdcSamplerChannel *channels)
{
    uint16_t i;

    if ((sampler == NULL) || (hadc == NULL) || (dma_buf == NULL)
        || (channels == NULL) || (frame_count == 0U) || (channel_count == 0U)
        || (channel_count > ADC_SAMPLER_MAX_CHANNELS)
        || ((uint32_t)frame_count * channel_count > 0xFFFFU))
    {
        return HAL_ERROR;
    }

    sampler->hadc = hadc;
    sampler->dma_buf = dma_buf;
    sampler->frame_count = frame_count;
    sampler->channel_count = channel_count;
    sampler->last_frame = 0xFFFFFFFFU;
    sampler->has_data = 0U;
    sampler->updated = 0U;
    sampler->started = 0U;

    for (i = 0U; i < channel_count; i++)
    {
        sampler->ch[i] = channels[i];
        Lpf1_Init(&sampler->ch[i].lpf, channels[i].lpf_alpha);
        sampler->raw[i] = 0U;
        sampler->value[i] = channels[i].offset;
    }

    return HAL_OK;
}

HAL_StatusTypeDef AdcSampler_Start(AdcSampler *sampler)
{
    uint32_t length;
    HAL_StatusTypeDef status;

    if ((sampler == NULL) || (sampler->hadc == NULL)
        || (sampler->hadc->DMA_Handle == NULL))
    {
        return HAL_ERROR;
    }

    if (sampler->started != 0U)
    {
        return HAL_OK;
    }

    length = (uint32_t)sampler->frame_count * sampler->channel_count;
    status = HAL_ADC_Start_DMA(sampler->hadc, (uint32_t *)sampler->dma_buf, length);
    if (status == HAL_OK)
    {
        sampler->started = 1U;
    }

    return status;
}

HAL_StatusTypeDef AdcSampler_Stop(AdcSampler *sampler)
{
    HAL_StatusTypeDef status;

    if ((sampler == NULL) || (sampler->hadc == NULL))
    {
        return HAL_ERROR;
    }

    if (sampler->started == 0U)
    {
        return HAL_OK;
    }

    status = HAL_ADC_Stop_DMA(sampler->hadc);
    sampler->started = 0U;
    return status;
}

void AdcSampler_Update(AdcSampler *sampler)
{
    DMA_HandleTypeDef *hdma;
    uint32_t total;
    uint32_t remain;
    uint32_t complete_frames;
    uint32_t frame;
    const uint16_t *frame_data;
    uint16_t i;

    if ((sampler == NULL) || (sampler->started == 0U)
        || (sampler->hadc == NULL) || (sampler->hadc->DMA_Handle == NULL))
    {
        return;
    }

    sampler->updated = 0U;
    hdma = sampler->hadc->DMA_Handle;
    total = (uint32_t)sampler->frame_count * sampler->channel_count;
    remain = __HAL_DMA_GET_COUNTER(hdma);

    if (remain > total)
    {
        remain = total;
    }

    /* 取最近一帧完整数据，避开 DMA 正在写入的半帧。 */
    complete_frames = (total - remain) / sampler->channel_count;
    if (complete_frames == 0U)
    {
        return;
    }

    frame = (complete_frames - 1U) % sampler->frame_count;
    if ((sampler->has_data != 0U) && (frame == sampler->last_frame))
    {
        return;
    }

    frame_data = &sampler->dma_buf[frame * sampler->channel_count];

    for (i = 0U; i < sampler->channel_count; i++)
    {
        float engineering;

        sampler->raw[i] = frame_data[i];
        engineering = (float)frame_data[i] * sampler->ch[i].scale
                      + sampler->ch[i].offset;
        sampler->value[i] = Lpf1_Update(&sampler->ch[i].lpf, engineering);
    }

    sampler->last_frame = frame;
    sampler->has_data = 1U;
    sampler->updated = 1U;
}

float AdcSampler_Get(const AdcSampler *sampler, uint16_t channel)
{
    if ((sampler == NULL) || (channel >= sampler->channel_count))
    {
        return 0.0f;
    }

    return sampler->value[channel];
}

uint16_t AdcSampler_GetRaw(const AdcSampler *sampler, uint16_t channel)
{
    if ((sampler == NULL) || (channel >= sampler->channel_count))
    {
        return 0U;
    }

    return sampler->raw[channel];
}

uint8_t AdcSampler_HasData(const AdcSampler *sampler)
{
    return (sampler != NULL) ? sampler->has_data : 0U;
}

uint8_t AdcSampler_IsUpdated(const AdcSampler *sampler)
{
    return (sampler != NULL) ? sampler->updated : 0U;
}
