#pragma once
#include "config.h"
#include "types.h"

// EEG / NeuroSky TGAM Brainwave Parser
bool eegInit();
bool eegRead(EegData &out);
