#ifndef POWER_PID_H
#define POWER_PID_H

#include <stdint.h>

/* 位置式 PID：测量值微分 + 条件积分抗饱和，dt 单位秒，输出范围由外部给定。 */
typedef struct
{
    float kp;
    float ki;
    float kd;
    float dt;
    float out_min;
    float out_max;
    float integral;
    float integral_min;
    float integral_max;
    float prev_measurement;
    float output;
    uint8_t initialized;
} Pid;

void Pid_Init(Pid *pid, float kp, float ki, float kd,
              float dt, float out_min, float out_max);
void Pid_Reset(Pid *pid);
void Pid_SetGains(Pid *pid, float kp, float ki, float kd);
void Pid_SetDt(Pid *pid, float dt);
void Pid_SetOutputLimits(Pid *pid, float out_min, float out_max);
void Pid_SetIntegralLimits(Pid *pid, float integral_min, float integral_max);
float Pid_Update(Pid *pid, float setpoint, float measurement);
float Pid_UpdateDt(Pid *pid, float setpoint, float measurement, float dt);

#endif
