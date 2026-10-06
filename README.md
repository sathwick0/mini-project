# SmartScaleGuard

Tamper detection for weighing scales — ESP8266 + LDR + webcam feed. Flask + Socket.IO.

---

## Deploy on Render (recommended)

### Step 1 — Push to GitHub
```bash
git init
git add .
git commit -m "initial"
git remote add origin https://github.com/<you>/smartscaleguard.git
git push -u origin main
```

### Step 2 — Create Web Service on Render
1. Go to https://render.com → New → Web Service
2. Connect your GitHub repo
3. Set these values:

| Field | Value |
|---|---|
| **Environment** | Python 3 |
| **Build Command** | `pip install -r requirements.txt` |
| **Start Command** | `gunicorn -w 1 --threads 100 app:app` |

> The `Procfile` in the repo sets this automatically.

### Step 3 — Set environment variables
In Render dashboard → Environment → Add:

| Key | Value |
|---|---|
| `SECRET_KEY` | any random string (e.g. `xk9f2mq8`) |

> Do NOT set `DB` — SQLite default is fine for a demo.

### Step 4 — Deploy
Click **Deploy**. Render provides HTTPS automatically.  
Your URL will be: `https://your-app-name.onrender.com`

### Step 5 — First login
Open the URL → login with **admin / admin123**  
(admin account created automatically on first start)

---

## After Deploying — Update ESP and Camera

### ESP8266 (`esp/esp.ino`)
Change line 6:
```cpp
const char* SERVER = "https://your-app-name.onrender.com";
```
Reflash the board.

### Camera script (`cam/cam.py`)
```bash
python cam/cam.py https://your-app-name.onrender.com <esp_chip_id>
```

---

## Run Locally (for testing)

```bash
pip install -r requirements.txt
python app.py
```
Open http://localhost:5000 → login: **admin / admin123**

### Test across networks with ngrok
```bash
ngrok http 5000
# paste the https://xxxx.ngrok.io URL into esp.ino SERVER and cam.py arg
```

---

## Test Without Hardware

First add a scale in the dashboard with ESP ID `a1b2c3`, then:

```bash
# Fake tamper alert
curl -X POST https://your-app-name.onrender.com/alert \
  -H "Content-Type: application/json" \
  -d '{"esp_id":"a1b2c3"}'

# Fake camera frame
curl -X POST https://your-app-name.onrender.com/frame/a1b2c3 \
  -H "Content-Type: image/jpeg" \
  --data-binary @sample.jpg
```

---

## ESP8266 Setup

### Wiring
```
3.3V ──── LDR ──── A0 ──── 10kΩ ──── GND
```
- Casing closed (dark) → A0 reads low (~0–200)  
- Casing open (light) → A0 reads high (~700–1023)

### Arduino Libraries needed
Install via Arduino IDE → Library Manager:
- **WiFiManager** by tzapu
- ESP8266HTTPClient and WiFiClientSecure are bundled with the ESP8266 board package

### First Boot
1. Flash `esp/esp.ino` with correct `SERVER` URL
2. ESP opens a Wi-Fi portal named **SmartScale**
3. Connect your phone to **SmartScale** → enter Wi-Fi credentials
4. ESP prints its Chip ID to Serial Monitor (115200 baud) — copy it
5. Go to dashboard → **+ Add Scale** → paste Chip ID as ESP ID

### LDR Calibration
1. Open Serial Monitor (115200), read `analogRead(A0)` with casing closed → note value
2. Read with casing open under room light → note value
3. Set `#define LIGHT` midway between both, reflash

---

## Important Notes

| Topic | Note |
|---|---|
| **One worker only** | `gunicorn -w 1` is required — frames live in RAM and are not shared between workers |
| **Free tier sleep** | Render free tier sleeps after 15 min of no traffic. Cold start takes ~30 s. Use cam.py to keep it awake. |
| **SQLite resets** | On Render, the disk resets on each redeploy. Alerts are lost. For permanent storage, set `DB` env var to a PostgreSQL URL from Render's database add-on. |
| **No auth on ESP endpoints** | `/alert` and `/frame` are public. Anyone knowing an ESP ID can post. Acceptable for a college mini project. |

---

## File Structure

```
smartscaleguard/
  app.py              # all Flask routes + Socket.IO
  models.py           # User, Scale, Alert models
  requirements.txt    # server pip packages
  Procfile            # Render/gunicorn start command
  templates/
    base.html         # navbar, dark CSS, socket.io JS
    login.html        # admin login
    home.html         # main dashboard
    view.html         # per-scale detail + alerts
  esp/esp.ino         # ESP8266 Arduino sketch
  cam/cam.py          # webcam JPEG poster
```
