import * as fs from "fs";
import * as path from "path";

interface TelemetryPoint {
  subject_id: string;
  gender: string;
  notes: string;
  uptime_ms: number;
  risk_level: string;
  risk_score: number;
  risk_alert: string;
  ppg_online: boolean;
  ppg_attached: boolean;
  ppg_hr: number;
  ppg_spo2: number;
  imu_online: boolean;
  imu_acc_g: number;
  imu_pitch: number;
  imu_roll: number;
  imu_fall: boolean;
  eeg_online: boolean;
  eeg_synced: boolean;
  eeg_sq: number;
  eeg_dar: number;
  eeg_delta: number;
  eeg_theta: number;
  gps_locked: boolean;
  gps_lat: number;
  gps_lng: number;
  gps_sats: number;
  is_clean_eeg: boolean;
}

const fileProfiles: Record<string, { gender: string; notes: string }> = {
  "1.txt": { gender: "man", notes: "Lansia umum" },
  "2.txt": { gender: "man", notes: "Riwayat Stroke Sebelumnya" },
  "3.txt": { gender: "women", notes: "Riwayat Stroke Sebelumnya" },
  "4.txt": { gender: "women", notes: "Lansia umum" },
  "5.txt": { gender: "women", notes: "Lansia umum" },
  "6.txt": { gender: "man", notes: "Lansia umum" },
  "7.txt": { gender: "man", notes: "Riwayat Stroke, depresi/stres keluarga" },
};

const outputDir = path.join(process.cwd(), "data", "dataset_pslugs_20260923");
if (!fs.existsSync(outputDir)) {
  fs.mkdirSync(outputDir, { recursive: true });
}

const allData: TelemetryPoint[] = [];

for (const [filename, profile] of Object.entries(fileProfiles)) {
  const filePath = path.join(process.cwd(), filename);
  if (!fs.existsSync(filePath)) continue;

  const content = fs.readFileSync(filePath, "utf-8");
  const lines = content.split("\n");
  const subjectId = `PSLUGS-${filename.replace(".txt", "")}`;

  for (const line of lines) {
    const trimmed = line.trim();
    if (trimmed.startsWith("{") && trimmed.endsWith("}")) {
      try {
        const d = JSON.parse(trimmed);
        const eeg = d.eeg || {};
        const ppg = d.ppg || {};
        const imu = d.imu || {};
        const risk = d.risk || {};
        const gps = d.gps || {};

        // Validasi clean EEG: SQ <= 50 (bukan lepas elektroda / SQ=200)
        const isCleanEeg = Boolean(eeg.online && eeg.synced && typeof eeg.sq === "number" && eeg.sq <= 50);

        allData.push({
          subject_id: subjectId,
          gender: profile.gender,
          notes: profile.notes,
          uptime_ms: d.uptime_ms || 0,
          risk_level: risk.level || "UNKNOWN",
          risk_score: risk.score || 0,
          risk_alert: risk.alert || "",
          ppg_online: Boolean(ppg.online),
          ppg_attached: Boolean(ppg.attached),
          ppg_hr: ppg.hr || 0,
          ppg_spo2: ppg.spo2 || 0,
          imu_online: Boolean(imu.online),
          imu_acc_g: imu.acc_g || 0,
          imu_pitch: imu.pitch || 0,
          imu_roll: imu.roll || 0,
          imu_fall: Boolean(imu.fall),
          eeg_online: Boolean(eeg.online),
          eeg_synced: Boolean(eeg.synced),
          eeg_sq: eeg.sq ?? 200,
          eeg_dar: eeg.dar || 0,
          eeg_delta: eeg.delta || 0,
          eeg_theta: eeg.theta || 0,
          gps_locked: Boolean(gps.locked),
          gps_lat: gps.lat || 0,
          gps_lng: gps.lng || 0,
          gps_sats: gps.sats || 0,
          is_clean_eeg: isCleanEeg,
        });
      } catch (err) {}
    }
  }
}

if (allData.length > 0) {
  // 1. Tulis Dataset Lengkap CSV & JSONL
  const csvHeader = Object.keys(allData[0]).join(",") + "\n";
  const csvRows = allData.map(row => Object.values(row).map(v => typeof v === "string" ? `"${v}"` : v).join(",")).join("\n");
  fs.writeFileSync(path.join(outputDir, "dataset_pslugs_full.csv"), csvHeader + csvRows);
  fs.writeFileSync(path.join(outputDir, "dataset_pslugs_full.jsonl"), allData.map(r => JSON.stringify(r)).join("\n"));

  // 2. Tulis Dataset Bersih (Hanya Clean EEG Signal Quality <= 50)
  const cleanData = allData.filter(d => d.is_clean_eeg);
  const cleanRows = cleanData.map(row => Object.values(row).map(v => typeof v === "string" ? `"${v}"` : v).join(",")).join("\n");
  fs.writeFileSync(path.join(outputDir, "dataset_pslugs_clean_eeg.csv"), csvHeader + cleanRows);
  fs.writeFileSync(path.join(outputDir, "dataset_pslugs_clean_eeg.jsonl"), cleanData.map(r => JSON.stringify(r)).join("\n"));

  console.log(`✅ Sukses Ekspor Dataset!`);
  console.log(`📊 Total Data Mentah : ${allData.length} baris`);
  console.log(`✨ Total Data Clean EEG (SQ <= 50) : ${cleanData.length} baris (${((cleanData.length / allData.length) * 100).toFixed(1)}%)`);
  console.log(`📁 Lokasi Output: ${outputDir}`);
}
