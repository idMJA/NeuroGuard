#include "risk.h"
#include <stdio.h>
#include <string.h>

// Temporal persistence filter state
static uint8_t consecutiveAnomalies = 0;
static float smoothedScore = 0.0f;
static const uint8_t PERSISTENCE_THRESHOLD_SAMPLES = 4; // Requires 4 consecutive seconds of true anomaly

RiskReport riskEval(const ImuData &imu, const PpgData &ppg, const EegData &eeg) {
    RiskReport report;
    report.score = 0;
    report.level = RISK_NORMAL;
    report.timestamp = millis();
    report.alertMsg[0] = '\0';

    char reasons[64] = "";
    uint8_t rawScore = 0;

    // 1. Fall Detection & Posture Collapse Evaluation
    if (imu.isOnline && imu.isFall) {
        rawScore += 50;
        strcat(reasons, "FALL ");
    }

    // 2. Cardiorespiratory Vitals Evaluation (PPG)
    if (ppg.isOnline && ppg.isAttached) {
        // Hypoxia check (< 92% SpO2)
        if (ppg.spo2 > 0 && ppg.spo2 < PPG_SPO2_HYPOXIA_PCT) {
            rawScore += 30;
            strcat(reasons, "HYPOXIA ");
        }

        // Arrhythmia / Severe Bradycardia or Tachycardia
        if (ppg.hr > 0 && (ppg.hr < PPG_HR_BRADY_BPM || ppg.hr > PPG_HR_TACHY_BPM)) {
            rawScore += 25;
            strcat(reasons, "ARRHYTHMIA ");
        }
    }

    // 3. Neurological Slowing & Stroke Biomarker Evaluation (EEG)
    // STRICT GATING: Only evaluate when signal quality is clean (SQ <= 50) and electrode is attached (SQ != 200)
    if (eeg.isOnline && eeg.isSynced && eeg.signalQuality <= 50) {
        // Delta-to-Alpha Ratio (DAR) elevation (Cerebral slowing index)
        if (eeg.dar >= EEG_DAR_ALERT_THRESH) {
            rawScore += 35;
            strcat(reasons, "EEG_DAR_HIGH ");
        }

        // High Delta power percentage (Pathological slow waves)
        uint32_t totalPower = eeg.delta + eeg.theta + eeg.lowAlpha + eeg.highAlpha + eeg.lowBeta + eeg.highBeta;
        if (totalPower > 0) {
            float deltaPct = ((float)eeg.delta / (float)totalPower) * 100.0f;
            if (deltaPct > EEG_DELTA_DOMINANCE_PCT) {
                rawScore += 20;
                strcat(reasons, "DELTA_DOMINANCE ");
            }
        }
    }

    if (rawScore > 100) rawScore = 100;

    // 4. TEMPORAL PERSISTENCE FILTER (Anti-False-Alarm Engine)
    // Instantaneous spikes (e.g. eye blinks, muscle twitch, touching headband) last 1-2s and are ignored.
    // True clinical stroke emergencies persist across multiple consecutive seconds.
    if (rawScore >= 30) {
        if (consecutiveAnomalies < 10) consecutiveAnomalies++;
    } else {
        if (consecutiveAnomalies > 0) consecutiveAnomalies--;
    }

    // Exponential moving average for smooth score transitions
    smoothedScore = (smoothedScore * 0.65f) + ((float)rawScore * 0.35f);
    uint8_t finalScore = (uint8_t)smoothedScore;

    // Immediate critical pass if a high-impact fall is confirmed
    bool isPersistentAnomaly = (consecutiveAnomalies >= PERSISTENCE_THRESHOLD_SAMPLES);
    bool isImmediateFall = (imu.isOnline && imu.isFall);

    if (isImmediateFall || (finalScore >= 60 && isPersistentAnomaly)) {
        report.level = RISK_CRITICAL;
        report.score = (finalScore > 60) ? finalScore : 75;
        if (strlen(reasons) == 0) strcpy(report.alertMsg, "CRITICAL EMERGENCY");
        else snprintf(report.alertMsg, sizeof(report.alertMsg), "%s", reasons);
    } else if (finalScore >= 30 && isPersistentAnomaly) {
        report.level = RISK_WARNING;
        report.score = finalScore;
        snprintf(report.alertMsg, sizeof(report.alertMsg), "%s", reasons);
    } else {
        report.level = RISK_NORMAL;
        report.score = (finalScore < 25) ? 0 : (finalScore / 2);
        strcpy(report.alertMsg, "NOMINAL");
    }

    return report;
}
