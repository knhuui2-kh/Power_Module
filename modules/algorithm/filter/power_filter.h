#ifndef POWER_FILTER_H
#define POWER_FILTER_H

#include <stdint.h>

/* 一阶低通：alpha = dt / (tau + dt)。alpha >= 1 立即跟随，alpha <= 0 直通。 */
typedef struct
{
    float alpha;
    float y;
    uint8_t ready;
} Lpf1;

void Lpf1_Init(Lpf1 *lpf, float alpha);
void Lpf1_Reset(Lpf1 *lpf);
float Lpf1_Update(Lpf1 *lpf, float x);

/* 滑动平均：存储区由调用方提供，不使用 malloc。 */
typedef struct
{
    float *buf;
    uint16_t size;
    uint16_t index;
    uint16_t count;
    float sum;
} MovAvg;

void MovAvg_Init(MovAvg *avg, float *buf, uint16_t size);
void MovAvg_Reset(MovAvg *avg);
float MovAvg_Update(MovAvg *avg, float x);

float Filter_Median3(float a, float b, float c);
uint16_t Filter_Median3U16(uint16_t a, uint16_t b, uint16_t c);

#endif
