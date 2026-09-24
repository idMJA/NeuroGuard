import serial
import serial.tools.list_ports
import json
import csv
import time
import os
from datetime import datetime

def find_esp32_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        # ESP32-S3 USB CDC usually shows as USB Serial Device or Espressif
        if "USB" in p.description or "Serial" in p.description or "Espressif" in p.description or "CH340" in p.description or "CP210" in p.description:
            return p.device
    if ports:
        return ports[0].device
    return None

def start_logging():
    port = find_esp32_port()
    if not port:
        print("❌ ESP32-S3 tidak ditemukan! Pastikan kabel USB sudah tercolok.")
        port_input = input("Masukkan nama port manual (misal COM3), atau tekan Enter untuk batal: ").strip()
        if not port_input:
            return
        port = port_input

    baud = 115200
    print(f"🔌 Menghubungkan ke {port} pada {baud} baud...")

    try:
        ser = serial.Serial(port, baud, timeout=2)
        time.sleep(2)
        print("✅ Terhubung!")
    except Exception as e:
        print(f"❌ Gagal membuka port {port}: {e}")
        return

    # Folder penyimpanan
    log_dir = os.path.join(os.path.dirname(__file__), "logs")
    os.makedirs(log_dir, exist_ok=True)

    timestamp_str = datetime.now().strftime("%Y%m%d_%H%M%S")
    jsonl_filename = os.path.join(log_dir, f"neuroguard_log_{timestamp_str}.jsonl")
    csv_filename = os.path.join(log_dir, f"neuroguard_log_{timestamp_str}.csv")

    print(f"\n📡 Memulai perekaman data telemetri...")
    print(f"📁 Simpan JSONL: {jsonl_filename}")
    print(f"📁 Simpan CSV  : {csv_filename}")
    print("--------------------------------------------------")
    print("Tekan Ctrl+C untuk berhenti merekam kapan saja.\n")

    # CSV Header
    csv_headers = [
        "timestamp", "uptime_ms", 
        "risk_level", "risk_score", 
        "ppg_hr", "ppg_spo2", "ppg_attached",
        "imu_acc_g", "imu_fall", 
        "eeg_dar", "eeg_sq", "eeg_delta", "eeg_theta",
        "gps_locked", "gps_sats"
    ]

    count = 0
    with open(jsonl_filename, "a", encoding="utf-8") as f_json, open(csv_filename, "a", newline="", encoding="utf-8") as f_csv:
        writer = csv.writer(f_csv)
        writer.writerow(csv_headers)

        try:
            while True:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue
                
                # Hanya tangkap baris yang merupakan JSON valid dari NeuroGuard
                if line.startswith("{") and line.endswith("}"):
                    try:
                        data = json.loads(line)
                        now_iso = datetime.now().isoformat()
                        
                        # Write to JSONL
                        record = {"logged_at": now_iso, "data": data}
                        f_json.write(json.dumps(record) + "\n")
                        f_json.flush()

                        # Extract for CSV
                        risk = data.get("risk", {})
                        ppg = data.get("ppg", {})
                        imu = data.get("imu", {})
                        eeg = data.get("eeg", {})
                        gps = data.get("gps", {})

                        row = [
                            now_iso,
                            data.get("uptime_ms", 0),
                            risk.get("level", ""),
                            risk.get("score", 0),
                            ppg.get("hr", 0),
                            ppg.get("spo2", 0),
                            ppg.get("attached", False),
                            imu.get("acc_g", 0.0),
                            imu.get("fall", False),
                            eeg.get("dar", 0.0),
                            eeg.get("sq", 0),
                            eeg.get("delta", 0),
                            eeg.get("theta", 0),
                            gps.get("locked", False),
                            gps.get("sats", 0)
                        ]
                        writer.writerow(row)
                        f_csv.flush()

                        count += 1
                        print(f"[{count}] 💾 Recorded | HR: {ppg.get('hr')} bpm | SpO2: {ppg.get('spo2')}% | DAR: {eeg.get('dar')} | Risk: {risk.get('level')}")

                    except json.JSONDecodeError:
                        pass # Ignore non-json debug prints

        except KeyboardInterrupt:
            print(f"\n🛑 Perekaman dihentikan oleh pengguna. Total {count} sampel disimpan!")
        finally:
            ser.close()

if __name__ == "__main__":
    start_logging()
