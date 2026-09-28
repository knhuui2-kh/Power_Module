#include "power_pid.h"

#include <stddef.h>

static float Pid_Clamp(float value, float min, float max)
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

void Pid_Init(Pid *pid, float kp, float ki, float kd,
              float dt, float out_min, float out_max)
{
    if (pid == NULL)
    {
        return;
    }

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->dt = (dt > 0.0f) ? dt : 1.0f;
    pid->out_min = out_min;
    pid->out_max = out_max;
    pid->integral_min = out_min;
    pid->integral_max = out_max;
    Pid_Reset(pid);
}

void Pid_Reset(Pid *pid)
{
    if (pid == NULL)
    {
        return;
    }

    pid->integral = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->output = Pid_Clamp(0.0f, pid->out_min, pid->out_max);
    pid->initialized = 0U;
}

void Pid_SetGains(Pid *pid, float kp, float ki, float kd)
{
    if (pid == NULL)
    {
        return;
    }

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}

void Pid_SetDt(Pid *pid, float dt)
{
    if ((pid != NULL) && (dt > 0.0f))
    {
        pid->dt = dt;
    }
}

void Pid_SetOutputLimits(Pid *pid, float out_min, float out_max)
{
    if ((pid == NULL) || (out_min > out_max))
    {
        return;
    }

    pid->out_min = out_min;
    pid->out_max = out_max;
    pid->output = Pid_Clamp(pid->output, out_min, out_max);

    if (pid->integral_min < out_min)
    {
        pid->integral_min = out_min;
    }

    if (pid->integral_max > out_max)
    {
        pid->integral_max = out_max;
    }

    if (pid->integral_min > pid->integral_max)
    {
        pid->integral_min = out_min;
        pid->integral_max = out_max;
    }
}

void Pid_SetIntegralLimits(Pid *pid, float integral_min, float integral_max)
{
    if ((pid == NULL) || (integral_min > integral_max))
    {
        return;
    }

    pid->integral_min = integral_min;
    pid->integral_max = integral_max;
    pid->integral = Pid_Clamp(pid->integral, integral_min, integral_max);
}

float Pid_Update(Pid *pid, float setpoint, float measurement)
{
    if (pid == NULL)
    {
        return 0.0f;
    }

    return Pid_UpdateDt(pid, setpoint, measurement, pid->dt);
}

float Pid_UpdateDt(Pid *pid, float setpoint, float measurement, float dt)
{
    float error;
    float proportional;
    float derivative;
    float candidate;
    float unsaturated;
    float output;

    if ((pid == NULL) || (dt <= 0.0f))
    {
        return (pid != NULL) ? pid->output : 0.0f;
    }

    if (pid->initialized == 0U)
    {
        pid->prev_measurement = measurement;
        pid->initialized = 1U;
    }

    error = setpoint - measurement;
    proportional = pid->kp * error;
    derivative = -pid->kd * (measurement - pid->prev_measurement) / dt;
    candidate = pid->integral + pid->ki * error * dt;
    candidate = Pid_Clamp(candidate, pid->integral_min, pid->integral_max);
    unsaturated = proportional + candidate + derivative;

    if (unsaturated > pid->out_max)
    {
        /* 正饱和：只有误差反向时才接受积分，避免积分继续增大 */
        if (error < 0.0f)
        {
            pid->integral = candidate;
        }
        output = pid->out_max;
    }
    else if (unsaturated < pid->out_min)
    {
        /* 负饱和：只有误差正向时才接受积分，避免积分继续减小 */
        if (error > 0.0f)
        {
            pid->integral = candidate;
        }
        output = pid->out_min;
    }
    else
    {
        pid->integral = candidate;
        output = unsaturated;
    }

    pid->integral = Pid_Clamp(pid->integral, pid->integral_min, pid->integral_max);
    pid->prev_measurement = measurement;
    pid->output = Pid_Clamp(output, pid->out_min, pid->out_max);

    return pid->output;
}
