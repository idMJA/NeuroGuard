#pragma once
#include "config.h"
#include "types.h"

// GPS Geospatial Tracker (NEO-6M)
bool gpsInit();
bool gpsRead(GpsData &out);
uint32_t gpsGetCharsProcessed();
uint32_t gpsGetFailedChecksum();
