import sys
import cv2
import requests
import time

# reads server url and esp id from command line args
server = sys.argv[1].rstrip('/')
esp_id = sys.argv[2]

# opens the default webcam
cap = cv2.VideoCapture(0)

# captures a frame, encodes it as jpeg and posts it to the server
while True:
    ret, frame = cap.read()

    # skip bad frames, keep going
    if not ret:
        time.sleep(1)
        continue

    frame = cv2.resize(frame, (320, 240))

    # compress and post, ignore errors
    _, buf = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 60])
    try:
        requests.post(
            server + '/frame/' + esp_id,
            data=buf.tobytes(),
            headers={'Content-Type': 'image/jpeg'},
            timeout=5
        )
    except Exception:
        pass

    time.sleep(0.2)
