# NeuroGuard AI — Comprehensive Project Dossier & Knowledge Export

**Export Date:** 2026-09-17  
**Workspace Root (Firmware):** `d:\Projects\NeuroGuard`  
**Workspace Root (Mobile App):** `d:\Projects\NeuroGuard-app`  
**Package Identifier:** `moe.mja.neuroguard`  
**Target Hardware:** ESP32-S3 Super Mini (Dual-Core 240MHz, USB CDC Native)  
**Target Mobile Device:** Samsung Galaxy S22 (`SM-S901E`) via Wireless ADB / BLE  

---

## 1. Executive Summary & Concept
* **Project Name:** NeuroGuard AI: Intelligent Wearable System for Early Stroke Risk Monitoring and Emergency Response.
* **Core Philosophy:** A smart medical wearable headband monitoring multi-parameter physiological biomarkers continuously. An onboard and mobile dual AI engine synthesizes the multi-modal streams to detect early neurological anomalies (ischemic slowing, acute hemorrhagic events, hypoxia, arrhythmia, impact falls) before irreparable brain tissue damage occurs.
* **Target Users:** Elderly individuals, hypertensive/diabetic patients, individuals with atrial fibrillation (AFib), prior TIA/stroke history, and high-risk patients living alone.

---

## 2. Hardware Architecture & Complete Pinout Mapping

### Microcontroller: ESP32-S3 Super Mini
* **Architecture:** Xtensa dual-core 32-bit LX7, 240 MHz.
* **Flashing Parameters:** `upload_speed = 460800` (reduced from 921600 to ensure stable USB CDC flashing), Native USB CDC enabled.

### Pinout Table
| ESP32-S3 Pin | Target Module | Target Module Pin | Protocol / Function |
| :--- | :--- | :--- | :--- |
| **5V / VBUS** | GPS NEO-6M, Buzzer | **VCC / 5V** | Main 5V power rail |
| **3V3** | MPU6050, MAX30105, HC-01 (EEG) | **VCC / 3.3V** | 3.3V logic & sensor power |
| **GND** | All Modules | **GND** | Common ground reference |
| **GPIO 8** | MPU6050 & MAX30105 | **SDA** | I2C Data bus (shared parallel) |
| **GPIO 9** | MPU6050 & MAX30105 | **SCL** | I2C Clock bus (shared parallel) |
| **GPIO 6** | GPS NEO-6M | **TX** | UART1 RX ESP32 (receives NMEA at 9600 baud) |
| **GPIO 7** | GPS NEO-6M | **RX** | UART1 TX ESP32 |
| **GPIO 1** | TGAM EEG / HC-01 | **TXD** | UART2 RX ESP32 (receives EEG packets at 57600 baud) |
| **GPIO 2** | TGAM EEG / HC-01 | **RXD** | UART2 TX ESP32 |
| **GPIO 3** | Active Buzzer | **(+) / Signal** | Emergency audio alarm trigger |
| **GPIO 4** | External Status LED | **Anode (+)** | Emergency status indicator |
| **GPIO 21** | Internal ESP32-S3 LED | - | Onboard status blinker |

### Physical Wearable Placement
1. **EEG Electrodes (TGAM/NeuroSky):** Frontal lobe contact (FP1 position according to the 10-20 international EEG standard) on the left forehead without hair interference. Reference clip attaches to the left earlobe.
2. **MAX30105 Optical PPG:** Optical sensor aperture flush against the forehead/temple skin to capture temporal pulse and blood volume oscillations.
3. **MPU6050 6-Axis IMU:** Positioned orthogonal to the head frame to differentiate upright posture vs acute fall impact.
4. **NEO-6M GPS:** Ceramic patch antenna pointing skyward. Note: The GY-NEO6MV2 only possesses a PPS (time pulse) LED which stays OFF indoors until locked with $\ge 4$ satellites.

---

## 3. Firmware Implementation (`d:\Projects\NeuroGuard`)

### Code Structure
* `include/config.h`: Central pin definitions, rates, and clinical safety thresholds.
* `src/main.cpp`: FreeRTOS dual-core task scheduling:
  * **Core 0 Task (`taskSensors`, 50 Hz):** High-speed IMU sampling, MAX30105 optical reading, and TGAM UART2 byte stream decoding.
  * **Core 1 Task (`taskProcess`, 5 Hz):** Risk assessment engine, GPS NMEA parsing, BLE advertising/notifications, and JSON telemetry stream.
* `src/risk.cpp`: Multi-parameter heuristic scoring & Anti-False-Alarm Temporal Persistence Filter:
  * Raw Score Calculation: Fall impact (+50), Hypoxia $\text{SpO}_2 < 92\%$ (+30), Arrhythmia $\text{HR} < 45$ or $> 130$ (+25), EEG DAR $\ge 3.5$ (+35), Delta Dominance $> 55\%$ (+20).
  * Temporal Filter: Requires $\ge 4$ consecutive seconds of abnormal readings to escalate alerts, rejecting eye blinks and motion artifacts.
* `src/ble.cpp`: Nordic UART Service (NUS) BLE server (Device Name: `NeuroGuard-AI`). Service UUID `6E400001-...`, TX Char `6E400003-...`. Transmits 1 Hz JSON telemetry.

---

## 4. Mobile App Modular Architecture (`d:\Projects\NeuroGuard-app`)

* **Package Identifier:** `moe.mja.neuroguard`
* **Clean File Tree:**
  ```
  lib/
  ├── models/
  │   └── telemetry.dart              # TelemetryData & AiFilteredResult models
  ├── services/
  │   ├── stroke_ml_model.dart        # 11-Dim Multi-Modal Neural Network (Option A)
  │   ├── gemini_service.dart         # Google Gemini 2.5 Flash Clinical API (Option C)
  │   └── telemetry_controller.dart   # ChangeNotifier managing BLE, AI filter & state
  ├── painters/
  │   ├── radial_gauge_painter.dart   # Brochure 180-deg arc gauge custom painter
  │   └── trend_painter.dart          # Time-series line chart custom painter
  ├── widgets/
  │   ├── risk_gauge_card.dart        # Risk score card with gauge & AI Doctor trigger
  │   ├── vitals_card.dart            # Vital signs card (EEG, HR, SpO2, Posture)
  │   ├── gps_card.dart               # Smart GPS indicator & Google Maps launcher
  │   ├── ai_assessment_dialog.dart   # Gemini AI Doctor assessment & FAST sheet
  │   └── sos_dialog.dart             # Emergency SOS confirmation & phone dispatch
  ├── screens/
  │   ├── dashboard_tab.dart          # Tab 1: Dashboard
  │   ├── history_tab.dart            # Tab 2: History & Event logs
  │   ├── alert_tab.dart              # Tab 3: Emergency Mode & SOS
  │   ├── profile_tab.dart            # Tab 4: Patient Profile & Hardware Status
  │   └── main_screen.dart            # Scaffold, AppBar, and BottomNavigationBar
  └── main.dart                       # App entry point & Theme setup (38 lines)
  ```

---

## 5. Dual AI Engine Implementation

### Engine 1: On-Device Multi-Modal Neural Network Classifier (`stroke_ml_model.dart`)
* **Type:** Vectorized Multi-Layer Perceptron (MLP) implemented in pure Dart for offline, zero-NDK execution ($< 0.2\text{ ms}$ latency).
* **Topology:** $11 \text{ Inputs} \rightarrow 8 \text{ Hidden (ReLU)} \rightarrow 1 \text{ Output (Sigmoid)}$.
* **Feature Vector (11 Dimensions):**
  1. `DAR`: Delta-to-Alpha Ratio (EEG cerebral slowing index)
  2. `Delta Ratio`: Pathological slow wave power dominance
  3. `Theta Ratio`: Sub-alpha power
  4. `Heart Rate`: In BPM
  5. `SpO2`: Blood oxygen saturation percentage
  6. `Fall Impact`: Posture collapse / sudden acceleration spike ($g$)
  7. `Age`: Patient age
  8. `Hypertension`: Binary clinical history
  9. `Diabetes`: Binary clinical history
  10. `AFib / Heart Disease`: Cardiovascular arrhythmia risk
  11. `Prior Stroke / TIA`: Cerebrovascular history
* **Explainable AI (Attribution):** Dynamically calculates and ranks the top contributing physiological factors (e.g. `Cerebral Slowing DAR: +32%`, `Hypertension Baseline: +15%`).

### Engine 2: Generative Clinical AI Assistant (`gemini_service.dart`)
* **Model:** Google Gemini 2.5 Flash via REST API (with built-in offline clinical protocol fallback).
* **Clinical Capabilities:**
  1. **Neurological Assessment:** Generates natural language analysis of current cerebral slowing and vital stability.
  2. **FAST Triage Protocol:** Evaluates Face drooping, Arm weakness, Speech slurring, and Time urgency.
  3. **Doctor Emergency Briefing:** 1-click generation of a medical-grade emergency briefing dispatched via WhatsApp or SMS, including exact GPS coordinates, biomarker readings, and patient history.

---

## 6. Real-Time Telemetry JSON Specification
Transmitted every 1,000 ms over BLE and Serial CDC:
```json
{
  "uptime_ms": 142529,
  "risk": {
    "level": "WARNING",
    "score": 50,
    "alert": "EEG_DAR_HIGH DELTA_DOMINANCE "
  },
  "ppg": {
    "online": true,
    "attached": true,
    "hr": 76,
    "spo2": 98
  },
  "imu": {
    "online": true,
    "acc_g": 1.02,
    "pitch": 80.1,
    "roll": -1.0,
    "fall": false
  },
  "eeg": {
    "online": true,
    "synced": true,
    "sq": 0,
    "dar": 3.82,
    "delta": 650000,
    "theta": 180000
  },
  "gps": {
    "locked": false,
    "lat": 0.0,
    "lng": 0.0,
    "sats": 0,
    "chars": 450,
    "chkerr": 0
  }
}
```
