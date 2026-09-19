#pragma once
#include "config.h"
#include "types.h"

// Risk Assessment & Emergency Fusion Engine
RiskReport riskEval(const ImuData &imu, const PpgData &ppg, const EegData &eeg);
