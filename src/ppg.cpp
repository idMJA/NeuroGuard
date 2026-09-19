#include "ppg.h"
#include <MAX30105.h>
#include <heartRate.h>
#include <Wire.h>

static MAX30105 maxSensor;
static bool initialized = false;

// Moving average buffer for Heart Rate
static const byte RATE_SIZE = 4;
static byte rates[RATE_SIZE];
static byte rateSpot = 0;
static long lastBeat = 0;
static float beatsPerMinute = 0;
static int beatAvg = 0;

// SpO2 calculation variables
static float dcRed = 0, dcIr = 0;
static float acRed = 0, acIr = 0;
static float spo2Calc = 98.0f;

bool ppgInit() {
    if (initialized) return true;

    // Ping I2C address 0x57 before initializing
    Wire.beginTransmission(0x57);
    if (Wire.endTransmission() != 0) {
        initialized = false;
        return false;
    }

    if (!maxSensor.begin(Wire, I2C_SPEED_FAST)) {
        initialized = false;
        return false;
    }

    // Configure MAX30105 for Biometric Pulse & SpO2 tracking
    byte ledBrightness = 0x3F; // Strong optical power
    byte sampleAverage = 4;    // Average 4 samples for smooth noise reduction
    byte ledMode = 2;          // 2 = Red + IR
    int sampleRate = 400;      // 400 Hz sampling rate
    int pulseWidth = 411;      // 411 us pulse width for high 18-bit ADC resolution
    int adcRange = 4096;       // 4096 ADC range

    maxSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    maxSensor.setPulseAmplitudeRed(0x3F);
    maxSensor.setPulseAmplitudeIR(0x3F);
    maxSensor.setPulseAmplitudeGreen(0);

    initialized = true;
    return true;
}

bool ppgRead(PpgData &out) {
    if (!initialized) {
        out.isOnline = false;
        out.isAttached = false;
        out.hr = 0;
        out.spo2 = 0;
        return false;
    }

    uint32_t irValue = maxSensor.getIR();
    uint32_t redValue = maxSensor.getRed();

    out.rawIr = irValue;
    out.rawRed = redValue;
    out.isOnline = true;

    // Detect skin/forehead contact
    if (irValue < PPG_FINGER_DETECT_IR) {
        out.isAttached = false;
        out.hr = 0;
        out.spo2 = 0;
        return true;
    }

    out.isAttached = true;

    // Heartbeat detection
    if (checkForBeat(irValue)) {
        long delta = millis() - lastBeat;
        lastBeat = millis();

        if (delta > 250 && delta < 2500) { // Valid HR range: 24 - 240 BPM
            beatsPerMinute = 60.0f / (delta / 1000.0f);

            rates[rateSpot++] = (byte)beatsPerMinute;
            rateSpot %= RATE_SIZE;

            // Compute running average
            beatAvg = 0;
            for (byte x = 0; x < RATE_SIZE; x++) {
                beatAvg += rates[x];
            }
            beatAvg /= RATE_SIZE;
        }
    }

    // Low-pass filter for SpO2 calculation (R = (AC_red / DC_red) / (AC_ir / DC_ir))
    dcRed = dcRed * 0.95f + (float)redValue * 0.05f;
    dcIr  = dcIr * 0.95f + (float)irValue * 0.05f;

    acRed = fabsf((float)redValue - dcRed);
    acIr  = fabsf((float)irValue - dcIr);

    if (dcRed > 0 && dcIr > 0 && acIr > 0) {
        float ratio = (acRed / dcRed) / (acIr / dcIr);
        // Empirical linear approximation formula: SpO2 = 110 - 25 * R
        float estSpo2 = 110.0f - (25.0f * ratio);
        if (estSpo2 > 100.0f) estSpo2 = 100.0f;
        if (estSpo2 < 70.0f)  estSpo2 = 70.0f;

        spo2Calc = spo2Calc * 0.9f + estSpo2 * 0.1f;
    }

    out.hr = (float)beatAvg;
    out.spo2 = spo2Calc;

    return true;
}
