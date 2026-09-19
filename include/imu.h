#pragma once
#include "config.h"
#include "types.h"

// IMU Sensor Driver & Posture / Fall Detection
bool imuInit();
bool imuRead(ImuData &out);
void imuResetFall();
