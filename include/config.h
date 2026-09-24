#pragma once
#include <Arduino.h>

/* ==========================================================================
 * NeuroGuard AI - Hardware Pinouts & System Configuration
 * Target: ESP32-S3 Super Mini
 * ========================================================================== */

// --- I2C Bus (IMU & PPG) ---
#define PIN_I2C_SDA         8
#define PIN_I2C_SCL         9
#define I2C_FREQ_HZ         400000

// --- GPS Module (NEO-6M on UART1) ---
#define PIN_GPS_RX          6       // GPIO 6 (Colok ke TX modul GPS)
#define PIN_GPS_TX          7       // GPIO 7 (Colok ke RX modul GPS)
#define GPS_BAUD_RATE       9600

// --- EEG Module (TGAM / NeuroSky on UART2) ---
#define PIN_EEG_RX          1       // GPIO 1 (Colok ke TXD modul Bluetooth HC-01)
#define PIN_EEG_TX          2       // GPIO 2 (Colok ke RXD modul Bluetooth HC-01)
#define EEG_BAUD_RATE       57600

// --- Actuators & Indicators ---
#define PIN_BUZZER          3       // GPIO 3
#define PIN_LED_STATUS      4       // GPIO 4 (dan LED internal 21)

// --- Task Frequencies & Timers (ms) ---
#define SENSOR_TASK_RATE_MS 20      // 50 Hz Core 0 loop
#define PROCESS_TASK_RATE_MS 200    // 5 Hz Core 1 loop
#define TELEMETRY_RATE_MS   1000    // 1 Hz Serial/JSON report

// --- Clinical & Risk Thresholds ---
// IMU / Fall Detection Thresholds
#define IMU_FALL_ACC_THRESH_G     2.7f    // Impact spike threshold (> 2.7g)
#define IMU_FALL_TILT_ANGLE_DEG   60.0f   // Posture tilt threshold post-impact
#define IMU_FREE_FALL_THRESH_G    0.4f    // Free fall before impact (< 0.4g)

// Biometric PPG Thresholds
#define PPG_HR_BRADY_BPM          45      // Severe Bradycardia
#define PPG_HR_TACHY_BPM          130     // Severe Tachycardia
#define PPG_SPO2_HYPOXIA_PCT      92      // Cerebral Hypoxia threshold (< 92%)
#define PPG_FINGER_DETECT_IR      3000    // Sensitive IR contact threshold (optimized for forehead / thin elderly skin)

// EEG Biomarker Thresholds (Neuro-vascular / Stroke index)
#define EEG_DAR_ALERT_THRESH      3.5f    // Delta-Alpha Ratio Slowing Index (> 3.5 = Severe slowing)
#define EEG_DELTA_DOMINANCE_PCT   55.0f   // Pathological Delta power proportion
