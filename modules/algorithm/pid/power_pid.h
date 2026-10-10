#ifndef POWER_PID_H
#define POWER_PID_H

#include <stdint.h>

/*
 * 增量式 PI：内部计算 Δu 并累加，对外返回绝对输出。
 * 不依赖采样周期参数，适用于固定控制周期；Ki 为每周期积分系数，改变控制频率需重新整定。
 */
typedef struct
{
    float kp;
    float ki;
    float out_min;
    float out_max;
    float output;
    float prev_error;
} Pid;

void Pid_Init(Pid *pid, float kp, float ki, float out_min, float out_max);
void Pid_Reset(Pid *pid);
void Pid_SetGains(Pid *pid, float kp, float ki);
void Pid_SetOutputLimits(Pid *pid, float out_min, float out_max);
void Pid_SetOutput(Pid *pid, float output);
float Pid_Update(Pid *pid, float setpoint, float measurement);

#endif
