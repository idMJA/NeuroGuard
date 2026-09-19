#include "gps.h"
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

static TinyGPSPlus gps;
static HardwareSerial gpsSerial(1); // Use UART1
static bool initialized = false;
static uint32_t lastCharTime = 0;

bool gpsInit() {
    if (initialized) return true;

    gpsSerial.begin(GPS_BAUD_RATE, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    initialized = true;
    return true;
}

bool gpsRead(GpsData &out) {
    if (!initialized) {
        out.isOnline = false;
        out.isLocked = false;
        return false;
    }

    // Process all incoming characters non-blockingly
    while (gpsSerial.available() > 0) {
        char c = gpsSerial.read();
        gps.encode(c);
        lastCharTime = millis();
    }

    // Detect if module is actively communicating
    out.isOnline = (millis() - lastCharTime < 3000);

    if (gps.location.isValid()) {
        out.lat = gps.location.lat();
        out.lng = gps.location.lng();
        out.isLocked = true;
    } else {
        out.lat = 0.0;
        out.lng = 0.0;
        out.isLocked = false;
    }

    if (gps.speed.isValid()) {
        out.speed = (float)gps.speed.kmph();
    } else {
        out.speed = 0.0f;
    }

    if (gps.altitude.isValid()) {
        out.altitude = (float)gps.altitude.meters();
    } else {
        out.altitude = 0.0f;
    }

    if (gps.satellites.isValid()) {
        out.sats = (uint8_t)gps.satellites.value();
    } else {
        out.sats = 0;
    }

    // Telemetry diagnostic: store raw char count in altitude temporarily if 0, or expose health
    return true;
}

uint32_t gpsGetCharsProcessed() {
    return gps.charsProcessed();
}

uint32_t gpsGetFailedChecksum() {
    return gps.failedChecksum();
}
