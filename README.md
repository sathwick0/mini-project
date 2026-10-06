# SmartScaleGuard

Tamper detection system for weighing scales using ESP8266 + LDR sensor and live webcam feed.

---

## Quick Start (Local)

### 1. Install Python 3.11
Download from https://www.python.org/downloads/ — check **"Add Python to PATH"** during install.

### 2. Install dependencies
```bash
cd "d:\iomp dashboard"
pip install -r requirements.txt
```

### 3. Run the server
```bash
python app.py
```
Opens on http://localhost:5000  
Login: **admin** / **admin123** (created automatically on first run)

---

## Test Without Hardware

First add a scale in the dashboard with ESP ID `A1B2C3`, then:

```bash
# Fake tamper alert
curl -X POST http://localhost:5000/alert \
  -H "Content-Type: application/json" \
  -d '{"esp_id":"A1B2C3"}'

# Fake camera frame (use any .jpg file)
curl -X POST http://localhost:5000/frame/A1B2C3 \
  -H "Content-Type: image/jpeg" \
  --data-binary @sample.jpg
```

---

## Run Camera Script

```bash
pip install opencv-python requests
python cam/cam.py http://localhost:5000 A1B2C3
```

---

## Cross-Network Testing (ngrok)

```bash
ngrok http 5000
# Use the https://xxxx.ngrok.io URL in:
#   esp/esp.ino  →  SERVER constant
#   cam.py arg   →  first argument
```

---

## Deploy on Render

1. Push this repo to GitHub
2. New Web Service on Render
   - Build: `pip install -r requirements.txt`
   - Start: `gunicorn -w 1 --threads 100 app:app`
3. Set env var `SECRET_KEY` (any random string)
4. HTTPS is provided automatically by Render

> **Important:** Run with ONE worker only (`-w 1`). The `frames` dict is in RAM and not shared between workers.

> **Free tier note:** Render free tier sleeps after 15 min of inactivity. Running cam.py keeps it awake.

---

## ESP8266 Setup

### Wiring
```
3.3V ──── LDR ──── A0 ──── 10kΩ ──── GND
```
Dark (casing closed) = low A0 reading (~0–200)  
Light (casing open) = high A0 reading (~700–1023)

### Arduino Libraries (install via Library Manager)
- WiFiManager by tzapu
- ESP8266HTTPClient (bundled with ESP8266 board package)
- WiFiClientSecure (bundled)

### Before Flashing
Edit `esp/esp.ino`:
```cpp
const char* SERVER = "https://your-app.onrender.com";
#define LIGHT 500   // adjust after calibration
```

### First Boot
1. ESP powers on → opens Wi-Fi portal named **"SmartScale"**
2. Connect phone to **SmartScale** → enter your Wi-Fi credentials
3. ESP connects and prints its Chip ID to Serial Monitor (115200 baud)
4. Copy the Chip ID → add scale in dashboard with that ESP ID

### LDR Calibration
1. Open Serial Monitor (115200 baud), add `Serial.println(analogRead(A0));` temporarily
2. Note reading with casing **closed** (expect < 200)
3. Note reading with casing **open** (expect > 700)
4. Set `LIGHT` midway between both values, reflash

---

## File Structure

```
smartscaleguard/
  app.py            # all Flask routes + Socket.IO
  models.py         # User, Scale, Alert models
  requirements.txt  # server dependencies
  templates/
    base.html       # navbar, flash messages, socket.io JS
    login.html      # admin login
    home.html       # scale cards + add/edit/delete
    view.html       # live video + alert history
  esp/esp.ino       # ESP8266 Arduino sketch
  cam/cam.py        # webcam → JPEG poster
```

---

## Known Limitation

`/alert` and `/frame` endpoints have no authentication. Anyone who knows an ESP ID can post to them. Acceptable for a college mini project.
