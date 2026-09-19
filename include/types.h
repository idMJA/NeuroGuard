#pragma once
#include <Arduino.h>

/* ==========================================================================
 * Data Types & System Structures
 * ========================================================================== */

// IMU Motion & Posture Data
struct ImuData {
    float ax, ay, az;       // Acceleration in m/s^2 or g
    float gx, gy, gz;       // Gyroscope in deg/s or rad/s
    float totalAcc;         // Net acceleration magnitude (g)
    float pitch, roll;      // Head tilt angles (degrees)
    bool isFall;            // Fall event detected flag
    bool isOnline;          // Sensor health status
};

// PPG Cardiorespiratory Data
struct PpgData {
    float hr;               // Heart Rate (BPM)
    float spo2;             // Blood Oxygen Saturation (%)
    uint32_t rawIr;         // Raw IR photodiode value
    uint32_t rawRed;        // Raw Red photodiode value
    bool isAttached;        // Skin/forehead contact detected
    bool isOnline;          // Sensor health status
};

// GPS Geospatial Tracking Data
struct GpsData {
    double lat;             // Latitude (degrees)
    double lng;             // Longitude (degrees)
    float speed;            // Speed over ground (km/h)
    float altitude;         // Altitude (meters)
    uint8_t sats;           // Number of tracked satellites
    bool isLocked;          // Fix status (true = valid fix)
    bool isOnline;          // Serial stream health
};

// EEG Neurological Waveform Data (TGAM / NeuroSky)
struct EegData {
    uint8_t signalQuality;  // 0 = Best, 200 = Poor/No contact
    uint32_t delta;         // 0.5 - 2.75 Hz (Deep sleep / Ischemic slowing)
    uint32_t theta;         // 3.5 - 6.75 Hz (Drowsiness / Encephalopathy)
    uint32_t lowAlpha;      // 7.5 - 9.25 Hz
    uint32_t highAlpha;     // 10.0 - 11.75 Hz
    uint32_t lowBeta;       // 13.0 - 16.75 Hz
    uint32_t highBeta;      // 17.0 - 29.75 Hz
    uint32_t lowGamma;      // 31.0 - 39.75 Hz
    uint32_t midGamma;      // 41.0 - 49.75 Hz
    uint8_t attention;      // 0 - 100
    uint8_t meditation;     // 0 - 100
    float dar;              // Delta-to-Alpha Ratio (DAR) biomarker
    bool isSynced;          // Valid packet lock
    bool isOnline;          // Stream active
};

// Risk Levels
enum RiskLevel {
    RISK_NORMAL = 0,        // Green: Normal baseline vitals
    RISK_WARNING = 1,       // Yellow: Anomalous biomarker or vitals drift
    RISK_CRITICAL = 2       // Red: Acute Stroke Risk or Fall Emergency
};

// Emergency Evaluation Output
struct RiskReport {
    RiskLevel level;
    uint8_t score;          // Composite score 0 - 100
    char alertMsg[64];      // Reason for alarm
    uint32_t timestamp;     // Millis timestamp
};

// Aggregated Sensor Snapshot for Inter-Task Communication
struct SensorSnapshot {
    ImuData imu;
    PpgData ppg;
    GpsData gps;
    EegData eeg;
    uint32_t timestamp;
};
