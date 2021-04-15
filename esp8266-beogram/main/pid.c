#include "pid.h"

void pid_init(struct pid* pid, 
        pid_value_t Kp, pid_value_t Ki, pid_value_t Kd,
        pid_value_t setpoint,
        pid_value_t output_lower_bound, pid_value_t output_upper_bound){

    pid->K.p = Kp;
    pid->K.i = Ki;
    pid->K.d = Kd;

    pid->setpoint = setpoint;
    pid->output_bounds.lower = output_lower_bound;
    pid->output_bounds.upper = output_upper_bound;

    pid->last.time = 0;
    pid->last.output = 0;
    pid->last.input = 0;

    pid->components.p = 0;
    pid->components.i = 0;
    pid->components.d = 0;
}

pid_value_t pid_update(struct pid* pid, pid_value_t input, int64_t time){
    if(!pid->last.time){
        pid->last.input = input;
        pid->last.output = pid->components.i = pid->output_bounds.lower;
        pid->last.time = time;

        return pid->last.output;
    }

    int64_t dt = time - pid->last.time;
    if(dt < 0){
        pid->last.time = time;
        return pid->last.output;
    }

    pid_value_t err = input - pid->setpoint;
    pid->components.p = -pid->K.p * err;

    pid->components.i -= pid->K.i * err * dt / 1000000.;
    if(pid->components.i > pid->output_bounds.upper)
        pid->components.i = pid->output_bounds.upper;
    if(pid->components.i < pid->output_bounds.lower)
        pid->components.i = pid->output_bounds.lower;

    pid_value_t d_input = input - pid->last.input;
    pid->components.d -= pid->K.d * d_input * 100000. / dt;

    pid_value_t output = pid->components.p + pid->components.i + pid->components.d;
    if(output > pid->output_bounds.upper)
        output = pid->output_bounds.upper;
    if(output < pid->output_bounds.lower)
        output = pid->output_bounds.lower;

    pid->last.input = input;
    pid->last.output = output;
    pid->last.time = time;

    return output;
}
