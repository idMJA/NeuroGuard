#pragma once
#include "config.h"
#include "types.h"

// WiFi SoftAP & Web Dashboard Server
bool netInit();
void netUpdate(const SensorSnapshot &snap, const RiskReport &risk);
