#include "eeg.h"
#include <HardwareSerial.h>

static HardwareSerial eegSerial(2); // Use UART2
static bool initialized = false;
static EegData currentData = {0};
static uint32_t lastPacketTime = 0;

// TGAM Parser State Machine
enum EegParserState {
    STATE_SYNC_1,
    STATE_SYNC_2,
    STATE_LENGTH,
    STATE_PAYLOAD,
    STATE_CHECKSUM
};

static EegParserState state = STATE_SYNC_1;
static uint8_t payloadLength = 0;
static uint8_t payload[256];
static uint8_t payloadIndex = 0;
static uint16_t checksumAccumulator = 0;

static void parsePayload(const uint8_t *buf, uint8_t len) {
    uint8_t i = 0;
    while (i < len) {
        uint8_t code = buf[i++];
        if (code == 0x02) { // Poor Signal Quality
            currentData.signalQuality = buf[i++];
        } else if (code == 0x04) { // eSense Attention
            currentData.attention = buf[i++];
        } else if (code == 0x05) { // eSense Meditation
            currentData.meditation = buf[i++];
        } else if (code == 0x83) { // ASIC EEG Power (8 frequency bands x 3 bytes)
            uint8_t vLength = buf[i++];
            if (vLength >= 24 && (i + 24 <= len)) {
                auto read24 = [&](uint8_t offset) -> uint32_t {
                    return ((uint32_t)buf[offset] << 16) | ((uint32_t)buf[offset + 1] << 8) | (uint32_t)buf[offset + 2];
                };

                currentData.delta     = read24(i);
                currentData.theta     = read24(i + 3);
                currentData.lowAlpha  = read24(i + 6);
                currentData.highAlpha = read24(i + 9);
                currentData.lowBeta   = read24(i + 12);
                currentData.highBeta  = read24(i + 15);
                currentData.lowGamma  = read24(i + 18);
                currentData.midGamma  = read24(i + 21);

                i += vLength;

                // Calculate Delta-to-Alpha Ratio (DAR) biomarker
                uint32_t totalAlpha = currentData.lowAlpha + currentData.highAlpha;
                if (totalAlpha > 0) {
                    currentData.dar = (float)currentData.delta / (float)totalAlpha;
                } else {
                    currentData.dar = 0.0f;
                }

                currentData.isSynced = true;
                lastPacketTime = millis();
            } else {
                i += vLength;
            }
        } else if (code >= 0x80) {
            // Multibyte code, skip
            uint8_t vLength = buf[i++];
            i += vLength;
        }
    }
}

bool eegInit() {
    if (initialized) return true;

    eegSerial.begin(EEG_BAUD_RATE, SERIAL_8N1, PIN_EEG_RX, PIN_EEG_TX);
    initialized = true;
    return true;
}

bool eegRead(EegData &out) {
    if (!initialized) {
        out.isOnline = false;
        out.isSynced = false;
        return false;
    }

    while (eegSerial.available() > 0) {
        uint8_t b = eegSerial.read();

        switch (state) {
            case STATE_SYNC_1:
                if (b == 0xAA) state = STATE_SYNC_2;
                break;

            case STATE_SYNC_2:
                if (b == 0xAA) {
                    state = STATE_LENGTH;
                } else {
                    state = STATE_SYNC_1;
                }
                break;

            case STATE_LENGTH:
                if (b > 169) { // TGAM standard max payload size
                    state = STATE_SYNC_1;
                } else {
                    payloadLength = b;
                    payloadIndex = 0;
                    checksumAccumulator = 0;
                    state = (payloadLength == 0) ? STATE_CHECKSUM : STATE_PAYLOAD;
                }
                break;

            case STATE_PAYLOAD:
                payload[payloadIndex++] = b;
                checksumAccumulator += b;
                if (payloadIndex >= payloadLength) {
                    state = STATE_CHECKSUM;
                }
                break;

            case STATE_CHECKSUM: {
                uint8_t expectedChecksum = (~(checksumAccumulator & 0xFF)) & 0xFF;
                if (b == expectedChecksum) {
                    parsePayload(payload, payloadLength);
                }
                state = STATE_SYNC_1;
                break;
            }
        }
    }

    currentData.isOnline = (millis() - lastPacketTime < 4000);
    out = currentData;
    return true;
}
