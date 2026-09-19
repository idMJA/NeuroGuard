#pragma once
#include "config.h"
#include "types.h"

// PPG / Biometric Sensor Driver (MAX30105)
bool ppgInit();
bool ppgRead(PpgData &out);
