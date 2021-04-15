#ifndef PID_H
#define PID_H

#include <stdint.h>

typedef float pid_value_t;

struct pid {
    struct {
        pid_value_t p;
        pid_value_t i;
        pid_value_t d;
    } K;

    pid_value_t setpoint;

    struct {
        pid_value_t lower;
        pid_value_t upper;
    } output_bounds;

    struct {
        int64_t time;
        pid_value_t input;
        pid_value_t output;
    } last;

    struct {
        pid_value_t p;
        pid_value_t i;
        pid_value_t d;
    } components;
};

void pid_init(struct pid* pid, 
        pid_value_t Kp, pid_value_t Ki, pid_value_t Kd,
        pid_value_t setpoint,
        pid_value_t output_lower_bound, pid_value_t output_upper_bound);

pid_value_t pid_update(struct pid* pid, pid_value_t input, int64_t time);

#endif
