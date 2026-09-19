import serial
import json
import time
import os
import sys
from datetime import datetime

# Configure UTF-8 for Windows console
if sys.platform == "win32":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

PORT = "COM7"
BAUD = 115200
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "data")
os.makedirs(OUTPUT_DIR, exist_ok=True)

csv_filename = os.path.join(OUTPUT_DIR, f"telemetry_session_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")

print("=" * 65)
print("[*] NEUROGUARD AI - REAL-TIME TELEMETRY DATASET RECORDER")
print("=" * 65)
print(f"Connecting to ESP32 on port {PORT} at {BAUD} baud...")
print(f"Saving dataset to: {csv_filename}\n")

headers = [
    "timestamp", "iso_time", "risk", "score", "hr", "spo2",
    "dar", "sq", "fall", "lat", "lng", "pitch", "roll"
]

try:
    ser = serial.Serial(PORT, BAUD, timeout=2.0)
    ser.dtr = True
    ser.rts = True
    time.sleep(1.0)
    ser.reset_input_buffer()
except Exception as e:
    print(f"[!] Error opening {PORT}: {e}")
    print("Please make sure PlatformIO Serial Monitor or other serial terminals are closed.")
    sys.exit(1)

print("[+] Connected to ESP32! Listening for live telemetry stream...\n")

with open(csv_filename, mode="w", newline="", encoding="utf-8") as f:
    f.write(",".join(headers) + "\n")
    f.flush()

    sample_count = 0
    start_time = time.time()

    try:
        while True:
            raw_line = ser.readline().decode("utf-8", errors="ignore").strip()
            if not raw_line:
                continue

            # Check if line contains JSON payload
            if "{" in raw_line and "}" in raw_line:
                json_start = raw_line.find("{")
                json_end = raw_line.rfind("}") + 1
                json_str = raw_line[json_start:json_end]

                try:
                    data = json.loads(json_str)
                    sample_count += 1
                    now_iso = datetime.now().isoformat()
                    now_epoch = time.time()

                    risk = data.get("risk", "NORM")
                    score = data.get("score", 0)
                    hr = data.get("hr", 0)
                    spo2 = data.get("spo2", 0)
                    dar = data.get("dar", 0.0)
                    sq = data.get("sq", 200)
                    fall = 1 if data.get("fall") else 0
                    lat = data.get("lat", 0.0)
                    lng = data.get("lng", 0.0)
                    pitch = data.get("pitch", 0.0)
                    roll = data.get("roll", 0.0)

                    row = [
                        f"{now_epoch:.3f}", now_iso, str(risk), str(score),
                        str(hr), str(spo2), f"{dar:.3f}", str(sq),
                        str(fall), str(lat), str(lng), f"{pitch:.2f}", f"{roll:.2f}"
                    ]

                    f.write(",".join(row) + "\n")
                    f.flush()

                    elapsed = time.time() - start_time
                    print(f"[{elapsed:05.1f}s | #{sample_count:04d}] "
                          f"HR: {hr:>3} BPM | SpO2: {spo2:>3}% | DAR: {dar:>5.2f} | SQ: {sq:>3} | "
                          f"Risk: {risk} ({score:>2}%) | Fall: {fall}")

                except json.JSONDecodeError:
                    pass
            else:
                # Print non-JSON debug lines
                if len(raw_line) > 3:
                    print(f"[DEBUG] {raw_line}")

    except KeyboardInterrupt:
        print("\n\n[-] Recording stopped.")
    finally:
        ser.close()
        print(f"[+] Dataset saved successfully! Total samples: {sample_count}")
        print(f"File location: {csv_filename}")
