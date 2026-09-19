#include "imu.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

static Adafruit_MPU6050 mpu;
static bool initialized = false;
static bool fallFlag = false;
static uint32_t lastFallTime = 0;

bool imuInit() {
    if (initialized) return true;

    // Ping I2C address 0x68 before initializing
    Wire.beginTransmission(0x68);
    if (Wire.endTransmission() != 0) {
        initialized = false;
        return false;
    }

    if (!mpu.begin(0x68, &Wire)) {
        initialized = false;
        return false;
    }

    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    initialized = true;
    return true;
}

bool imuRead(ImuData &out) {
    if (!initialized) {
        out.isOnline = false;
        out.isFall = false;
        return false;
    }

    sensors_event_t a, g, temp;
    if (!mpu.getEvent(&a, &g, &temp)) {
        out.isOnline = false;
        return false;
    }

    out.isOnline = true;
    out.ax = a.acceleration.x;
    out.ay = a.acceleration.y;
    out.az = a.acceleration.z;
    out.gx = g.gyro.x;
    out.gy = g.gyro.y;
    out.gz = g.gyro.z;

    // Acceleration magnitude normalized to g (1g ≈ 9.81 m/s^2)
    float accMag = sqrtf(out.ax * out.ax + out.ay * out.ay + out.az * out.az) / 9.80665f;
    out.totalAcc = accMag;

    // Posture angles in degrees
    out.pitch = atan2f(out.ax, sqrtf(out.ay * out.ay + out.az * out.az)) * 180.0f / M_PI;
    out.roll  = atan2f(out.ay, sqrtf(out.ax * out.ax + out.az * out.az)) * 180.0f / M_PI;

    // Fall detection logic:
    // 1. High acceleration impact spike
    // 2. High posture tilt angle
    float tilt = fabsf(out.pitch) > fabsf(out.roll) ? fabsf(out.pitch) : fabsf(out.roll);
    if (accMag > IMU_FALL_ACC_THRESH_G && tilt > IMU_FALL_TILT_ANGLE_DEG) {
        fallFlag = true;
        lastFallTime = millis();
    }

    // Auto clear fall flag after 10 seconds if not reset
    if (fallFlag && (millis() - lastFallTime > 10000)) {
        fallFlag = false;
    }

    out.isFall = fallFlag;
    return true;
}

void imuResetFall() {
    fallFlag = false;
}
