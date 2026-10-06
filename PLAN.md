# SmartScaleGuard - PLAN.md

Read this fully, then build step by step (section 9). All decisions are made here.

---

## 1. Overview

An LDR sits inside each scale casing. Closed = dark. If the casing is opened, light hits the LDR and the ESP8266 sends ONE thing to the server: its own unique id (chip id), as a tamper alert.
The server finds the scale that owns that ESP id and shows the alert in that scale's card, live.
A laptop webcam next to each scale posts JPEG frames to the server, which shows them as a live video on that scale's page. No AI, no recording.

```
[ESP8266 + LDR] --HTTPS POST /alert {esp_id}--+
                                               +--> [ Flask server (public) ] --Socket.IO--> [ Admin browser ]
[Laptop webcam cam.py] --HTTPS POST /frame/<esp_id>--+
```

- ESP, laptop and dashboard are on different networks. ESP and laptop only make OUTGOING requests to the public server, so no port forwarding is needed.
- One scale = one ESP = one webcam laptop. The `esp_id` is what links all three.
- No heartbeat, no online/offline status, no tokens. Keep it simple.

---

## 2. Folder tree

```
smartscaleguard/
  app.py            # all routes + socketio (~100 lines)
  models.py         # User, Scale, Alert (~25 lines)
  requirements.txt
  README.md
  templates/
    base.html       # navbar, bootstrap, flash messages, beep + socket js
    login.html
    home.html       # scale cards + add form
    view.html       # one scale: live video + its alerts
  esp/esp.ino       # ESP8266 sketch (~40 lines)
  cam/cam.py        # webcam poster (~20 lines)
```

No utils/, services/, config files, Docker, tests.

---

## 3. Database (Flask-SQLAlchemy, SQLite default)

| table | columns |
|---|---|
| User  | id, name (unique), pw (werkzeug hash) |
| Scale | id, name, location, esp_id (unique, string) |
| Alert | id, sc_id (FK Scale.id), time (UTC), acked (bool, default False) |

- To use MySQL later, only change the `DB` uri line in app.py.
- On first start, app.py creates tables and, if no User exists, creates `admin` / `admin123`.
- Times are stored and shown in UTC (label the column "Time (UTC)").

---

## 4. Routes

Browser routes need login (Flask-Login). ESP and cam routes are public.

| method | path | who | what |
|---|---|---|---|
| GET, POST | /login | browser | show form, check password |
| GET | /logout | browser | log out |
| GET | / | browser | home.html: all scales as cards + add form |
| POST | /add | browser | form: name, location, esp_id. Reject duplicate esp_id with a flash message |
| POST | /edit/<id> | browser | form: name, location, esp_id |
| POST | /delete/<id> | browser | delete scale + its alerts + its frame from memory |
| GET | /view/<id> | browser | view.html: live video + this scale's alerts (last 50) |
| POST | /ack/<id> | browser | form button: acked=True, redirect back to previous page |
| POST | /alert | ESP | JSON `{"esp_id": "A1B2C3"}`. Find Scale by esp_id. Unknown id -> 404. Else save Alert, then emit socket event `alert` with `{"sc_id": 3, "name": "Scale A"}`. Return `{"ok": true}` |
| POST | /frame/<esp_id> | cam.py | raw JPEG body. Save into dict `frames[esp_id]` (RAM only). Return `{"ok": true}` |
| GET | /video/<esp_id> | browser | MJPEG stream (`multipart/x-mixed-replace; boundary=frame`), yields `frames[esp_id]` every 0.2 s. If no frame yet, wait/yield nothing and keep looping |

Socket.IO: one server -> browser event only: `alert`. Nothing else.

Note: `frames` lives in one process, so run ONE worker only (see section 10).

---

## 5. ESP8266 firmware (esp/esp.ino)

Libraries: WiFiManager, ESP8266HTTPClient, WiFiClientSecure.
No ArduinoJson — the JSON body is one short hardcoded string.

Wiring: 3.3V -> LDR -> A0 -> 10k resistor -> GND.
Dark (closed) = low reading. Light (open) = high reading.

Code logic:
1. Top constants: `SERVER` (public https url) and `LIGHT` threshold (start with 500).
2. `setup()`: start Serial, `WiFiManager().autoConnect("SmartScale")` (phone portal sets Wi-Fi at each location, nothing hardcoded). Build `id = String(ESP.getChipId(), HEX)` and print it on Serial — the admin copies it into the dashboard. Set `open = analogRead(A0) > LIGHT` so a boot with light on does not fire a false alert.
3. `loop()`: every 200 ms read A0. If light now and `open` was false -> call `send()`. Save the new `open` state. (Alert only on dark -> light change, never repeated.)
4. `send()`: WiFiClientSecure with `setInsecure()`, POST `SERVER/alert`, header `Content-Type: application/json`, body `{"esp_id":"<id>"}`. If it fails, retry once after 500 ms.

Note: `setInsecure()` skips certificate check. Fine for a college project — full TLS check can run out of memory on ESP8266.

---

## 6. Camera script (cam/cam.py)

Run: `python cam.py <server_url> <esp_id>`
Libraries: opencv-python, requests. Args from `sys.argv`, no argparse.

Loop forever:
1. read a frame from `cv2.VideoCapture(0)`, if it fails sleep 1 s and continue
2. resize to 320x240
3. `cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, 60])`
4. `requests.post(server + "/frame/" + esp_id, data=bytes, headers={"Content-Type": "image/jpeg"}, timeout=5)` inside try/except, ignore errors
5. sleep 0.2 s (about 5 fps)

---

## 7. Dashboard pages (Bootstrap 5 from CDN, plain look)

**base.html**: navbar (SmartScaleGuard | Logout), flash messages, `{% block body %}`. Also holds the small JS used by home and view:
- connect socket.io (CDN client)
- on `alert` event: play a short beep with the browser AudioContext oscillator (no mp3 file needed), show a red dismissible banner "Tamper alert: <name>", then `location.reload()` after 1 second so the page shows fresh data from DB.
- Note: browsers allow sound only after the user has clicked once on the page. Acceptable.

**login.html**: centered Bootstrap card with username, password, Login button.

**home.html**:
- "Add scale" form at top: name, location, esp id fields.
- One Bootstrap card per scale (grid layout): shows name, location, esp id, count of unacked alerts.
  - Card border/background turns red (`border-danger`) if it has unacked alerts, and lists its latest unacked alerts each with an "Ack" button (small form POST /ack/<id>).
  - Buttons: View (link to /view/<id>), Delete (form with `onsubmit="return confirm('Delete?')"`).
  - Edit: a `<details>` element inside the card holding a small inline form (name, location, esp_id, Save). No modal, no JS needed.

**view.html**: scale name + location heading, `<img src="/video/<esp_id>" width="320">` on the left, table of this scale's alerts (Time UTC | Acked / Ack button) on the right, Back link to /.

---

## 8. Libraries

| library | replaces |
|---|---|
| Flask-Login | manual sessions and login guards |
| Flask-SQLAlchemy | raw SQL |
| Flask-SocketIO + simple-websocket | custom live-update code |
| werkzeug.security | custom password hashing |
| gunicorn | Flask dev server in production |
| Bootstrap 5 (CDN) | custom CSS |
| WiFiManager (ESP) | hardcoded Wi-Fi and reconnect code |
| ESP8266HTTPClient, WiFiClientSecure | raw sockets / TLS on ESP |
| opencv-python, requests (laptop) | camera access, JPEG encoding, HTTP |

requirements.txt (server only):
```
flask
flask-sqlalchemy
flask-login
flask-socketio
simple-websocket
gunicorn
```

---

## 9. Style rules and build order

**Naming**: short, simple, student style.
- Classes: `User`, `Scale`, `Alert`
- Variables: `db`, `sc`, `al`, `frames`, `sio`
- Functions: `login`, `logout`, `home`, `view`, `add`, `edit`, `delete`, `ack`, `alert`, `frame`, `video`
- Files exactly as in section 2

**No**: type hints, docstrings, service/manager classes, helper modules, unused code, logging setup, commented-out blocks.
**Comments**: write a short, clear comment for every function/method. Explain what it does in plain words — enough for another developer to understand quickly without reading the body. Keep it simple and natural (student style). Do not write docstrings, multi-line explanations, or AI-style prose. Do not comment obvious single lines unless they are genuinely confusing. Comments are for development only and will be removed before final submission.
**Size**: app.py ~100 lines, models.py ~25, esp.ino ~40, cam.py ~20, each template under 60 lines.

Build order (stop and verify after each step):
1. requirements.txt, models.py, app.py skeleton with auto-create tables and admin seed.
   Check: `python app.py` starts, `site.db` is created, no error.
2. Login / logout and base.html.
   Check: sign in as admin / admin123.
3. Add / edit / delete scale on home.html.
   Check: card appears, edit works, delete removes it.
4. POST /alert with Socket.IO, ack buttons, red card highlight.
   Check: with home open, run the alert curl below — banner and beep appear, card turns red. Unknown esp_id returns 404. Ack clears the red.
5. POST /frame and GET /video, view.html.
   Check: post a jpg with curl, open /view/<id> and see it.
6. cam.py.
   Check: live webcam shows in /view/<id>.
7. esp.ino.
   Check: Serial shows chip id, add scale with that id, cover/uncover LDR, alert appears on that scale's card.
8. README with run, deploy and test steps from section 10.

---

## 10. Run, deploy, test

**Run locally**
```
pip install -r requirements.txt
python app.py     # uses socketio.run(app, host="0.0.0.0", port=5000, allow_unsafe_werkzeug=True)
```
Open http://localhost:5000, sign in with admin / admin123.

**Test across networks (demo)**: `ngrok http 5000`, put the https url in esp.ino `SERVER` constant and as the first arg to cam.py.

**Deploy (Render or Railway)**
- Build cmd: `pip install -r requirements.txt`
- Start cmd: `gunicorn -w 1 --threads 100 app:app` (ONE worker only — frames are in RAM)
- Set env var `SECRET_KEY` (any random string). HTTPS is provided by the host.
- Note: free tiers sleep when idle and free disk may wipe on redeploy, so SQLite data can reset. For a viva demo, run locally with ngrok — safest option. For permanent data, use the host's database add-on and set its URL in `DB`.

**Test without hardware**
```bash
# first add a scale with esp id A1B2C3 in the dashboard

curl -X POST http://localhost:5000/alert \
  -H "Content-Type: application/json" \
  -d '{"esp_id":"A1B2C3"}'

curl -X POST http://localhost:5000/frame/A1B2C3 \
  -H "Content-Type: image/jpeg" \
  --data-binary @sample.jpg
```

**LDR calibration**: open Serial Monitor (115200 baud). Read `analogRead(A0)` with casing closed and open. Set `LIGHT` midway between the two values. If the range is too narrow, swap the 10k resistor for a 4.7k or 22k.

Known limitation (acceptable for a mini project): `/alert` and `/frame` have no password — anyone who knows an esp_id could post to them.

---

## 11. Do NOT build

AI or vision tamper detection, 3D digital twin, heartbeat, online/offline status, tokens or API keys, checksums, multiple admin roles, user registration, email/SMS, charts, video recording, Docker, tests, config loaders, argparse.

---

## 12. Comment Style Guide

> Agent instruction: write a short, clear comment above every function and method in the project.
> The comment should say what the function does — one line, plain English, student tone.
> Do NOT write docstrings, multi-line blocks, or AI-style prose.
> Do NOT add comments for obvious single lines (e.g. `db.session.commit()`).
> Do NOT use a fixed/verbatim list — use judgment based on the function's actual job.
> These comments exist for development clarity only. Remove them all before final submission.
> There are NO tokens or API keys in this project — do not add any token-check comments or code.

### app.py — one comment per function, examples:

```python
# sets up flask, db, login manager and socketio

# creates db tables and adds default admin if none exists

# in-memory store for latest jpeg frame per esp_id

# shows login form and checks credentials on submit

# loads all scales and their unacked alert counts for home page

# adds a new scale, rejects duplicate esp_id

# updates name, location and esp_id for an existing scale

# removes a scale, its alerts and its cached frame

# shows a single scale's live video and last 50 alerts

# marks one alert as acknowledged and redirects back

# receives tamper alert from esp, saves it and notifies browser

# stores the latest jpeg frame for a scale in memory

# streams the latest frame as an mjpeg feed to the browser

# starts the server with socketio
```

### models.py — one comment per class:

```python
# stores the single admin account

# represents one physical scale with its esp id

# records each tamper event; acked turns true when dismissed
```

### cam.py — one comment per logical block:

```python
# reads server url and esp id from command line args

# opens the default webcam

# captures a frame, encodes it as jpeg and posts it to the server
```

### esp.ino — one comment per function/section:

```cpp
// constants: server url and light level threshold

// connects to wifi using a phone captive portal

// prints chip id to serial so admin can register this scale

// sets initial open state so boot in light does not trigger alert

// checks ldr every 200ms and fires send() on dark-to-light change

// posts the esp chip id to the server as a tamper alert
```

### base.html JS block — one comment per step:

```javascript
// connect to the socket.io server
// when alert fires: play a beep, show a banner, reload the page
// use audiocontext to make the beep sound without any audio file
```

### home.html and view.html — one comment per major HTML section:

```html
<!-- form to add a new scale -->
<!-- grid of scale cards, one per scale -->
<!-- card turns red and lists alerts if any are unacked -->
<!-- inline edit form, no javascript needed -->
<!-- ack and delete action buttons -->

<!-- scale name, location and back link -->
<!-- live mjpeg feed from the scale camera -->
<!-- table of this scale's tamper alerts -->
```
