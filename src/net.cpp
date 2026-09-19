#include "net.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

static WebServer server(80);
static SensorSnapshot latestSnap = {0};
static RiskReport latestRisk = {RISK_NORMAL, 0, "NOMINAL", 0};

// Embedded Mobile-First Dark Theme Dashboard HTML
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>NeuroGuard AI - Stroke Monitor</title>
  <style>
    :root {
      --bg: #0f172a; --card: #1e293b; --text: #f8fafc; --muted: #94a3b8;
      --green: #10b981; --yellow: #f59e0b; --red: #ef4444; --cyan: #06b6d4; --border: #334155;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 14px; }
    .header { text-align: center; margin-bottom: 14px; }
    .header h1 { font-size: 1.35rem; color: #38bdf8; }
    .header p { font-size: 0.8rem; color: var(--muted); }
    
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 10px; margin-bottom: 12px; }
    .card { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 12px; }
    .card-title { font-size: 0.75rem; text-transform: uppercase; color: var(--muted); letter-spacing: 0.5px; margin-bottom: 6px; }
    
    .risk-banner { border-radius: 12px; padding: 14px; text-align: center; margin-bottom: 12px; font-weight: bold; transition: all 0.3s; }
    .risk-normal { background: rgba(16, 185, 129, 0.15); border: 1px solid var(--green); color: var(--green); }
    .risk-warning { background: rgba(245, 158, 11, 0.2); border: 1px solid var(--yellow); color: var(--yellow); }
    .risk-critical { background: rgba(239, 68, 68, 0.25); border: 2px solid var(--red); color: var(--red); animation: pulse 1s infinite; }
    @keyframes pulse { 0% { transform: scale(1); } 50% { transform: scale(1.02); } 100% { transform: scale(1); } }
    
    .val-large { font-size: 1.7rem; font-weight: 700; }
    .val-unit { font-size: 0.8rem; color: var(--muted); }
    .status-badge { display: inline-block; padding: 2px 6px; border-radius: 4px; font-size: 0.7rem; font-weight: bold; }
    .badge-on { background: #065f46; color: #34d399; }
    .badge-off { background: #7f1d1d; color: #fca5a5; }
    
    .gps-btn { display: block; width: 100%; text-align: center; background: #2563eb; color: white; padding: 10px; border-radius: 8px; text-decoration: none; font-size: 0.85rem; font-weight: bold; margin-top: 6px; }
  </style>
</head>
<body>
  <div class="header">
    <h1>🧠 NeuroGuard AI</h1>
    <p>Wearable Stroke Early Risk Telemetry</p>
  </div>

  <div id="riskBox" class="risk-banner risk-normal">
    <div style="font-size: 0.8rem; text-transform: uppercase;">Status Risiko</div>
    <div id="riskLevel" style="font-size: 1.4rem; margin: 4px 0;">NORMAL (0%)</div>
    <div id="riskMsg" style="font-size: 0.75rem;">NOMINAL</div>
  </div>

  <div class="grid">
    <div class="card">
      <div class="card-title">💓 Detak Jantung</div>
      <div class="val-large"><span id="hr">--</span> <span class="val-unit">BPM</span></div>
      <div id="ppgBadge" class="status-badge badge-off" style="margin-top:4px;">No Finger</div>
    </div>
    <div class="card">
      <div class="card-title">🩸 SpO2 Oksigen</div>
      <div class="val-large"><span id="spo2">--</span> <span class="val-unit">%</span></div>
      <div style="font-size: 0.75rem; color: var(--muted); margin-top:4px;">Cerebral Saturation</div>
    </div>
  </div>

  <div class="grid">
    <div class="card">
      <div class="card-title">⚡ EEG Biomarker (DAR)</div>
      <div class="val-large" style="color: var(--cyan);"><span id="dar">--</span></div>
      <div style="font-size: 0.75rem; color: var(--muted); margin-top:4px;">SQ: <span id="sq">--</span> (0=Best)</div>
    </div>
    <div class="card">
      <div class="card-title">📐 Postur & Gerak</div>
      <div style="font-size: 0.95rem; font-weight: bold;">Acc: <span id="acc">--</span> g</div>
      <div style="font-size: 0.8rem; color: var(--muted); margin-top:4px;">Tilt: <span id="pitch">--</span>° / <span id="roll">--</span>°</div>
      <div id="fallBadge" class="status-badge badge-on" style="margin-top:4px;">Normal Posture</div>
    </div>
  </div>

  <div class="card" style="margin-bottom: 12px;">
    <div class="card-title">📍 Lokasi Darurat (GPS)</div>
    <div style="font-size: 0.85rem;">Koordinat: <span id="coords">Mencari Sinyal Satelit...</span></div>
    <div style="font-size: 0.75rem; color: var(--muted); margin-top:2px;">Satelit Terkunci: <span id="sats">0</span></div>
    <a id="mapLink" href="#" target="_blank" class="gps-btn" style="display:none;">Buka Peta Google Maps</a>
  </div>

  <script>
    async function updateData() {
      try {
        const res = await fetch('/api/data');
        const d = await res.json();
        
        // Risk Update
        const rBox = document.getElementById('riskBox');
        const rLvl = document.getElementById('riskLevel');
        const rMsg = document.getElementById('riskMsg');
        rLvl.innerText = `${d.risk.level} (${d.risk.score}%)`;
        rMsg.innerText = d.risk.alert;
        rBox.className = 'risk-banner ' + (d.risk.level === 'CRITICAL' ? 'risk-critical' : (d.risk.level === 'WARNING' ? 'risk-warning' : 'risk-normal'));

        // PPG Update
        document.getElementById('hr').innerText = d.ppg.attached ? d.ppg.hr.toFixed(0) : '--';
        document.getElementById('spo2').innerText = d.ppg.attached ? d.ppg.spo2.toFixed(1) : '--';
        const pBadge = document.getElementById('ppgBadge');
        pBadge.innerText = d.ppg.attached ? 'Terpasang' : 'Lepas';
        pBadge.className = 'status-badge ' + (d.ppg.attached ? 'badge-on' : 'badge-off');

        // EEG Update
        document.getElementById('dar').innerText = d.eeg.synced ? d.eeg.dar.toFixed(2) : '--';
        document.getElementById('sq').innerText = d.eeg.online ? d.eeg.sq : 'Offline';

        // IMU Update
        document.getElementById('acc').innerText = d.imu.online ? d.imu.acc_g.toFixed(2) : '--';
        document.getElementById('pitch').innerText = d.imu.online ? d.imu.pitch.toFixed(0) : '--';
        document.getElementById('roll').innerText = d.imu.online ? d.imu.roll.toFixed(0) : '--';
        const fBadge = document.getElementById('fallBadge');
        if (d.imu.fall) {
          fBadge.innerText = '⚠️ JATUH TERDETEKSI!';
          fBadge.className = 'status-badge badge-off';
        } else {
          fBadge.innerText = 'Postur Normal';
          fBadge.className = 'status-badge badge-on';
        }

        // GPS Update
        const coords = document.getElementById('coords');
        const mLink = document.getElementById('mapLink');
        document.getElementById('sats').innerText = d.gps.sats;
        if (d.gps.locked) {
          coords.innerText = `${d.gps.lat.toFixed(5)}, ${d.gps.lng.toFixed(5)}`;
          mLink.href = `https://maps.google.com/?q=${d.gps.lat},${d.gps.lng}`;
          mLink.style.display = 'block';
        } else {
          coords.innerText = 'Mencari Satelit GPS...';
          mLink.style.display = 'none';
        }
      } catch (e) {
        console.log("Telemetry fetch err:", e);
      }
    }
    setInterval(updateData, 800);
    updateData();
  </script>
</body>
</html>
)rawliteral";

static void handleRoot() {
    server.send(200, "text/html", DASHBOARD_HTML);
}

static void handleApiData() {
    JsonDocument doc;

    doc["uptime"] = latestSnap.timestamp;

    JsonObject r = doc["risk"].to<JsonObject>();
    r["level"] = (latestRisk.level == RISK_CRITICAL) ? "CRITICAL" : (latestRisk.level == RISK_WARNING ? "WARNING" : "NORMAL");
    r["score"] = latestRisk.score;
    r["alert"] = latestRisk.alertMsg;

    JsonObject ppg = doc["ppg"].to<JsonObject>();
    ppg["online"] = latestSnap.ppg.isOnline;
    ppg["attached"] = latestSnap.ppg.isAttached;
    ppg["hr"] = latestSnap.ppg.hr;
    ppg["spo2"] = latestSnap.ppg.spo2;

    JsonObject imu = doc["imu"].to<JsonObject>();
    imu["online"] = latestSnap.imu.isOnline;
    imu["acc_g"] = latestSnap.imu.totalAcc;
    imu["pitch"] = latestSnap.imu.pitch;
    imu["roll"] = latestSnap.imu.roll;
    imu["fall"] = latestSnap.imu.isFall;

    JsonObject eeg = doc["eeg"].to<JsonObject>();
    eeg["online"] = latestSnap.eeg.isOnline;
    eeg["synced"] = latestSnap.eeg.isSynced;
    eeg["sq"] = latestSnap.eeg.signalQuality;
    eeg["dar"] = latestSnap.eeg.dar;

    JsonObject gps = doc["gps"].to<JsonObject>();
    gps["locked"] = latestSnap.gps.isLocked;
    gps["lat"] = latestSnap.gps.lat;
    gps["lng"] = latestSnap.gps.lng;
    gps["sats"] = latestSnap.gps.sats;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

bool netInit() {
    // Start WiFi Access Point
    WiFi.mode(WIFI_AP);
    bool apOk = WiFi.softAP("NeuroGuard-AI", "neuroguard123");

    server.on("/", handleRoot);
    server.on("/api/data", handleApiData);
    server.begin();

    return apOk;
}

void netUpdate(const SensorSnapshot &snap, const RiskReport &risk) {
    latestSnap = snap;
    latestRisk = risk;
    server.handleClient();
}
