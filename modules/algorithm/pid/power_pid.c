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

void Pid_Init(Pid *pid, float kp, float ki, float out_min, float out_max)
{
    if (pid == NULL)
    {
        return;
    }

    pid->kp = kp;
    pid->ki = ki;
    pid->out_min = out_min;
    pid->out_max = out_max;
    Pid_Reset(pid);
}

void Pid_Reset(Pid *pid)
{
    if (pid == NULL)
    {
        return;
    }

    pid->output = Pid_Clamp(0.0f, pid->out_min, pid->out_max);
    pid->prev_error = 0.0f;
}

void Pid_SetGains(Pid *pid, float kp, float ki)
{
    if (pid == NULL)
    {
        return;
    }

    pid->kp = kp;
    pid->ki = ki;
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
}

void Pid_SetOutput(Pid *pid, float output)
{
    if (pid == NULL)
    {
        return;
    }

    pid->output = Pid_Clamp(output, pid->out_min, pid->out_max);
}

float Pid_Update(Pid *pid, float setpoint, float measurement)
{
    float error;
    float delta;

    if (pid == NULL)
    {
        return 0.0f;
    }

    error = setpoint - measurement;
    delta = pid->kp * (error - pid->prev_error) + pid->ki * error;
    pid->prev_error = error;
    pid->output = Pid_Clamp(pid->output + delta, pid->out_min, pid->out_max);

    return pid->output;
}
