#pragma once
#include "config.h"
#include "types.h"

// Bluetooth Low Energy (BLE) Nordic UART Service
bool bleInit();
void bleUpdate(const SensorSnapshot &snap, const RiskReport &risk);
bool bleIsConnected();
