import { SerialPort } from "serialport";
import { ReadlineParser } from "@serialport/parser-readline";
import * as fs from "fs";
import * as path from "path";

async function main() {
  const ports = await SerialPort.list();
  console.log("🔍 Mencari port ESP32-S3...");
  
  let targetPort = "";
  for (const p of ports) {
    if (
      p.manufacturer?.includes("Espressif") ||
      p.description?.includes("USB") ||
      p.description?.includes("Serial") ||
      p.pnpId?.includes("USB")
    ) {
      targetPort = p.path;
      break;
    }
  }

  if (!targetPort && ports.length > 0) {
    targetPort = ports[0].path;
  }

  if (!targetPort) {
    console.error("❌ ESP32-S3 tidak ditemukan! Pastikan kabel USB tercolok.");
    process.exit(1);
  }

  console.log(`🔌 Menghubungkan ke ${targetPort} pada 115200 baud...`);

  const port = new SerialPort({
    path: targetPort,
    baudRate: 115200,
  });

  const parser = port.pipe(new ReadlineParser({ delimiter: "\r\n" }));

  const logDir = path.join(process.cwd(), "tools", "logs");
  if (!fs.existsSync(logDir)) {
    fs.mkdirSync(logDir, { recursive: true });
  }

  const now = new Date();
  const timestampStr = now.toISOString().replace(/[:.]/g, "-");
  const csvFile = path.join(logDir, `neuroguard_log_${timestampStr}.csv`);
  const jsonlFile = path.join(logDir, `neuroguard_log_${timestampStr}.jsonl`);

  const csvHeaders = "timestamp,uptime_ms,risk_level,risk_score,ppg_hr,ppg_spo2,ppg_attached,imu_acc_g,imu_fall,eeg_dar,eeg_sq,eeg_delta,eeg_theta,gps_locked,gps_sats\n";
  fs.writeFileSync(csvFile, csvHeaders);

  console.log(`\n📡 Memulai perekaman data telemetri (Bun runtime)...`);
  console.log(`📁 Simpan CSV  : ${csvFile}`);
  console.log(`📁 Simpan JSONL: ${jsonlFile}`);
  console.log("--------------------------------------------------");

  let count = 0;

  parser.on("data", (line: string) => {
    const trimmed = line.trim();
    if (trimmed.startsWith("{") && trimmed.endsWith("}")) {
      try {
        const data = JSON.parse(trimmed);
        const isoTime = new Date().toISOString();

        // Write JSONL
        fs.appendFileSync(jsonlFile, JSON.stringify({ logged_at: isoTime, data }) + "\n");

        // Write CSV
        const risk = data.risk || {};
        const ppg = data.ppg || {};
        const imu = data.imu || {};
        const eeg = data.eeg || {};
        const gps = data.gps || {};

        const csvRow = [
          isoTime,
          data.uptime_ms || 0,
          risk.level || "",
          risk.score || 0,
          ppg.hr || 0,
          ppg.spo2 || 0,
          ppg.attached || false,
          imu.acc_g || 0.0,
          imu.fall || false,
          eeg.dar || 0.0,
          eeg.sq || 0,
          eeg.delta || 0,
          eeg.theta || 0,
          gps.locked || false,
          gps.sats || 0
        ].join(",") + "\n";

        fs.appendFileSync(csvFile, csvRow);

        count++;
        console.log(`[${count}] 💾 Recorded | HR: ${ppg.hr} bpm | SpO2: ${ppg.spo2}% | DAR: ${eeg.dar} | Risk: ${risk.level}`);
      } catch (e) {
        // Ignore non-json lines
      }
    }
  });

  port.on("error", (err) => {
    console.error("❌ Error Serial Port:", err.message);
  });
}

main();
