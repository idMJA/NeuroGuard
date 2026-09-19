#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "config.h"
#include "types.h"
#include "imu.h"
#include "ppg.h"
#include "gps.h"
#include "eeg.h"
#include "risk.h"
#include "net.h"
#include "ble.h"

// FreeRTOS Synchronization Handles
static SemaphoreHandle_t dataMutex = nullptr;
static SensorSnapshot sharedData = {0};
static RiskReport currentRisk = {RISK_NORMAL, 0, "INITIALIZING", 0};

// Actuator control state
static void setAlertState(RiskLevel level) {
    static uint32_t lastToggle = 0;
    static bool toggleState = false;
    uint32_t now = millis();

    switch (level) {
        case RISK_CRITICAL:
            // Fast strobe alarm
            if (now - lastToggle >= 100) {
                lastToggle = now;
                toggleState = !toggleState;
                digitalWrite(PIN_LED_STATUS, toggleState ? LOW : HIGH); // Active LOW on some Super Mini
                digitalWrite(PIN_BUZZER, toggleState ? HIGH : LOW);
            }
            break;

        case RISK_WARNING:
            // Medium warning blink
            digitalWrite(PIN_BUZZER, LOW);
            if (now - lastToggle >= 300) {
                lastToggle = now;
                toggleState = !toggleState;
                digitalWrite(PIN_LED_STATUS, toggleState ? LOW : HIGH);
            }
            break;

        case RISK_NORMAL:
        default:
            // Heartbeat blink (visible indicator that firmware is running)
            digitalWrite(PIN_BUZZER, LOW);
            if (now - lastToggle >= 500) {
                lastToggle = now;
                toggleState = !toggleState;
                digitalWrite(PIN_LED_STATUS, toggleState ? LOW : HIGH);
            }
            break;
    }
}

// Telemetry output in structured JSON
static void sendTelemetry(const SensorSnapshot &snap, const RiskReport &risk) {
    JsonDocument doc;

    doc["uptime_ms"] = snap.timestamp;

    // Risk Evaluation
    JsonObject r = doc["risk"].to<JsonObject>();
    r["level"] = (risk.level == RISK_CRITICAL) ? "CRITICAL" : (risk.level == RISK_WARNING ? "WARNING" : "NORMAL");
    r["score"] = risk.score;
    r["alert"] = risk.alertMsg;

    // Biometrics (PPG)
    JsonObject ppg = doc["ppg"].to<JsonObject>();
    ppg["online"] = snap.ppg.isOnline;
    ppg["attached"] = snap.ppg.isAttached;
    ppg["hr"] = snap.ppg.hr;
    ppg["spo2"] = snap.ppg.spo2;

    // Motion & Posture (IMU)
    JsonObject imu = doc["imu"].to<JsonObject>();
    imu["online"] = snap.imu.isOnline;
    imu["acc_g"] = snap.imu.totalAcc;
    imu["pitch"] = snap.imu.pitch;
    imu["roll"] = snap.imu.roll;
    imu["fall"] = snap.imu.isFall;

    // Brainwave Biomarkers (EEG)
    JsonObject eeg = doc["eeg"].to<JsonObject>();
    eeg["online"] = snap.eeg.isOnline;
    eeg["synced"] = snap.eeg.isSynced;
    eeg["sq"] = snap.eeg.signalQuality;
    eeg["dar"] = snap.eeg.dar;
    eeg["delta"] = snap.eeg.delta;
    eeg["theta"] = snap.eeg.theta;

    // Geospatial Coordinates (GPS)
    JsonObject gps = doc["gps"].to<JsonObject>();
    gps["locked"] = snap.gps.isLocked;
    gps["lat"] = snap.gps.lat;
    gps["lng"] = snap.gps.lng;
    gps["sats"] = snap.gps.sats;
    gps["chars"] = gpsGetCharsProcessed();
    gps["chkerr"] = gpsGetFailedChecksum();

    serializeJson(doc, Serial);
    Serial.println();
}

/* ==========================================================================
 * Core 0 Task: High-Frequency Sensor Acquisition
 * ========================================================================== */
void taskSensors(void *pvParameters) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(SENSOR_TASK_RATE_MS); // 50 Hz

    for (;;) {
        ImuData localImu = {0};
        PpgData localPpg = {0};
        EegData localEeg = {0};

        // Sample IMU & PPG over I2C
        imuRead(localImu);
        ppgRead(localPpg);

        // Sample EEG over UART2
        eegRead(localEeg);

        // Safely update shared snapshot
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            sharedData.imu = localImu;
            sharedData.ppg = localPpg;
            sharedData.eeg = localEeg;
            sharedData.timestamp = millis();
            xSemaphoreGive(dataMutex);
        }

        vTaskDelayUntil(&lastWakeTime, frequency);
    }
}

/* ==========================================================================
 * Core 1 Task: GPS, Risk Evaluation & Telemetry
 * ========================================================================== */
void taskProcess(void *pvParameters) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(PROCESS_TASK_RATE_MS); // 5 Hz
    uint32_t lastTelemetryTime = 0;

    for (;;) {
        GpsData localGps = {0};
        SensorSnapshot snapshot = {0};

        // Parse GPS NMEA stream non-blockingly
        gpsRead(localGps);

        // Retrieve local snapshot from shared buffer
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            sharedData.gps = localGps;
            snapshot = sharedData;
            xSemaphoreGive(dataMutex);
        }

        // Evaluate multi-parameter stroke risk
        currentRisk = riskEval(snapshot.imu, snapshot.ppg, snapshot.eeg);

        // Update actuators (Buzzer & LED)
        setAlertState(currentRisk.level);

        // Update Web Dashboard (WiFi) & BLE Telemetry
        netUpdate(snapshot, currentRisk);
        bleUpdate(snapshot, currentRisk);

        // Periodic telemetry reporting
        if (millis() - lastTelemetryTime >= TELEMETRY_RATE_MS) {
            lastTelemetryTime = millis();
            sendTelemetry(snapshot, currentRisk);
        }

        vTaskDelayUntil(&lastWakeTime, frequency);
    }
}

/* ==========================================================================
 * Setup & Task Initialization
 * ========================================================================== */
void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0); // Never block if USB CDC serial buffer isn't drained by PC

    // Wait up to 2.5s for USB CDC Serial to attach
    uint32_t startWait = millis();
    while (!Serial && (millis() - startWait < 2500)) {
        delay(10);
    }
    delay(200);

    Serial.println();
    Serial.println("==================================================");
    Serial.println("   NeuroGuard AI - Wearable Stroke Monitor");
    Serial.println("   Firmware Running on ESP32-S3 Super Mini");
    Serial.println("==================================================");

    // Initialize Actuator GPIOs
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);
    digitalWrite(PIN_BUZZER, LOW);

    // Initialize I2C Bus with short timeout to prevent blocking if unplugged
    Wire.setTimeOut(30);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);

    // Initialize Drivers (Graceful non-blocking checks)
    bool imuOk = imuInit();
    Serial.printf("[INIT] MPU6050 IMU:   %s\n", imuOk ? "READY" : "OFFLINE/NOT CONNECTED");

    bool ppgOk = ppgInit();
    Serial.printf("[INIT] MAX30105 PPG:  %s\n", ppgOk ? "READY" : "OFFLINE/NOT CONNECTED");

    bool gpsOk = gpsInit();
    Serial.printf("[INIT] NEO-6M GPS:    %s\n", gpsOk ? "READY" : "OFFLINE/NOT CONNECTED");

    bool eegOk = eegInit();
    Serial.printf("[INIT] TGAM EEG:      %s\n", eegOk ? "READY" : "OFFLINE/FAILED");

    // Initialize WiFi Web Dashboard (Access Point)
    bool netOk = netInit();
    Serial.printf("[INIT] Web Dashboard: %s (SSID: NeuroGuard-AI | IP: 192.168.4.1)\n", netOk ? "READY" : "FAILED");

    // Initialize BLE Nordic UART Service
    bool bleOk = bleInit();
    Serial.printf("[INIT] BLE Service:   %s (Device: NeuroGuard-AI)\n", bleOk ? "ADVERTISING" : "FAILED");

    // Create Mutex for thread-safe memory sharing
    dataMutex = xSemaphoreCreateMutex();

    // Create FreeRTOS Tasks pinned to respective cores
    xTaskCreatePinnedToCore(
        taskSensors,        // Task function
        "TaskSensors",      // Task name
        4096,               // Stack depth
        nullptr,            // Parameters
        2,                  // Priority
        nullptr,            // Task handle
        0                   // Core 0
    );

    xTaskCreatePinnedToCore(
        taskProcess,        // Task function
        "TaskProcess",      // Task name
        6144,               // Stack depth
        nullptr,            // Parameters
        1,                  // Priority
        nullptr,            // Task handle
        1                   // Core 1
    );

    Serial.println("[INIT] FreeRTOS Tasks Dispatched to Core 0 & Core 1.");
    Serial.println("[SYSTEM] System Operational. Telemetry Active.");
    Serial.println("--------------------------------------------------");
}

void loop() {
    // Empty: Execution handled entirely by FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}
