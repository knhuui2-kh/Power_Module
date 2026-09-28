#include "power_filter.h"

#include <stddef.h>

static float Filter_Clamp(float value, float min, float max)
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

void Lpf1_Init(Lpf1 *lpf, float alpha)
{
    if (lpf == NULL)
    {
        return;
    }

    lpf->alpha = Filter_Clamp(alpha, 0.0f, 1.0f);
    lpf->y = 0.0f;
    lpf->ready = 0U;
}

void Lpf1_Reset(Lpf1 *lpf)
{
    if (lpf == NULL)
    {
        return;
    }

    lpf->y = 0.0f;
    lpf->ready = 0U;
}

float Lpf1_Update(Lpf1 *lpf, float x)
{
    if (lpf == NULL)
    {
        return x;
    }

    if ((lpf->alpha <= 0.0f) || (lpf->alpha >= 1.0f) || (lpf->ready == 0U))
    {
        lpf->y = x;
        lpf->ready = 1U;
    }
    else
    {
        lpf->y += lpf->alpha * (x - lpf->y);
    }

    return lpf->y;
}

void MovAvg_Init(MovAvg *avg, float *buf, uint16_t size)
{
    uint16_t i;

    if (avg == NULL)
    {
        return;
    }

    avg->buf = buf;
    avg->size = (buf != NULL) ? size : 0U;
    avg->index = 0U;
    avg->count = 0U;
    avg->sum = 0.0f;

    for (i = 0U; i < avg->size; i++)
    {
        buf[i] = 0.0f;
    }
}

void MovAvg_Reset(MovAvg *avg)
{
    uint16_t i;

    if ((avg == NULL) || (avg->buf == NULL))
    {
        return;
    }

    avg->index = 0U;
    avg->count = 0U;
    avg->sum = 0.0f;

    for (i = 0U; i < avg->size; i++)
    {
        avg->buf[i] = 0.0f;
    }
}

float MovAvg_Update(MovAvg *avg, float x)
{
    if ((avg == NULL) || (avg->buf == NULL) || (avg->size == 0U))
    {
        return x;
    }

    if (avg->count < avg->size)
    {
        avg->sum += x;
        avg->count++;
    }
    else
    {
        avg->sum -= avg->buf[avg->index];
        avg->sum += x;
    }

    avg->buf[avg->index] = x;
    avg->index++;
    if (avg->index >= avg->size)
    {
        avg->index = 0U;
    }

    return avg->sum / (float)avg->count;
}

float Filter_Median3(float a, float b, float c)
{
    float t;

    if (a > b)
    {
        t = a;
        a = b;
        b = t;
    }

    if (b > c)
    {
        b = c;
    }

    return (a > b) ? a : b;
}

uint16_t Filter_Median3U16(uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t t;

    if (a > b)
    {
        t = a;
        a = b;
        b = t;
    }

    if (b > c)
    {
        b = c;
    }

    return (a > b) ? a : b;
}
