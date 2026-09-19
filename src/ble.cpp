#include "ble.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

// Standard Nordic UART Service (NUS) UUIDs
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static BLEServer *pServer = nullptr;
static BLECharacteristic *pTxCharacteristic = nullptr;
static bool deviceConnected = false;
static bool oldDeviceConnected = false;
static uint32_t lastNotifyTime = 0;

class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

class RxCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String rxValue = pCharacteristic->getValue().c_str();
        if (rxValue.length() > 0) {
            Serial.printf("[BLE RX] %s\n", rxValue.c_str());
        }
    }
};

bool bleInit() {
    BLEDevice::init("NeuroGuard-AI");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    // TX Characteristic (Notify to Phone)
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // RX Characteristic (Receive from Phone)
    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRxCharacteristic->setCallbacks(new RxCallbacks());

    pService->start();

    // Start Advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    return true;
}

void bleUpdate(const SensorSnapshot &snap, const RiskReport &risk) {
    // Handle reconnection
    if (!deviceConnected && oldDeviceConnected) {
        delay(200);
        pServer->startAdvertising();
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    // Stream telemetry every 1 second if client is connected
    if (deviceConnected && (millis() - lastNotifyTime >= 1000)) {
        lastNotifyTime = millis();

        JsonDocument doc;
        doc["risk"] = (risk.level == RISK_CRITICAL) ? "CRIT" : (risk.level == RISK_WARNING ? "WARN" : "NORM");
        doc["score"] = risk.score;
        doc["hr"] = snap.ppg.isAttached ? (int)snap.ppg.hr : 0;
        doc["spo2"] = snap.ppg.isAttached ? (int)snap.ppg.spo2 : 0;
        doc["dar"] = snap.eeg.isSynced ? snap.eeg.dar : 0.0f;
        doc["sq"] = snap.eeg.signalQuality;
        doc["fall"] = snap.imu.isFall;
        doc["gps_online"] = snap.gps.isOnline;
        doc["sats"] = snap.gps.sats;
        if (snap.gps.isLocked) {
            doc["lat"] = snap.gps.lat;
            doc["lng"] = snap.gps.lng;
        }

        String payload;
        serializeJson(doc, payload);
        payload += "\n";

        pTxCharacteristic->setValue((uint8_t*)payload.c_str(), payload.length());
        pTxCharacteristic->notify();
    }
}

bool bleIsConnected() {
    return deviceConnected;
}
