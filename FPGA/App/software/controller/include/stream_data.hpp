#pragma once

#include <stdint.h>

enum StreamId_t {
    StreamIdStatus = 1,
    StreamIdAdc2 = 2,
    StreamIdMotion = 3
};

struct StreamDataStatus_t {
    uint32_t error_flags;
    uint32_t fault_flags;
};

struct StreamDataAdc2_t {
    uint16_t dc48v_voltage;
    uint16_t dribble_current;
};

struct StreamDataMotion_t {
    uint16_t performance_counter;
    int16_t accelerometer[3];
    int16_t gyroscope[3];
    int16_t encoder_pulse_count[4];
    int16_t motor_current_d[4];
    int16_t motor_current_q[4];
    int16_t motor_current_ref_q[4];
    int16_t motor_power_5;
};