import os
import time
from flask import Flask, render_template, request, redirect, jsonify, Response, flash, url_for
from flask_login import LoginManager, login_user, logout_user, login_required, current_user
from flask_socketio import SocketIO
from werkzeug.security import generate_password_hash, check_password_hash
from models import db, User, Scale, Alert

# sets up flask, db, login manager and socketio
app = Flask(__name__)
app.config['SECRET_KEY'] = os.environ.get('SECRET_KEY', 'dev-secret-change-me')
app.config['SQLALCHEMY_DATABASE_URI'] = os.environ.get('DB', 'sqlite:///site.db')
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
db.init_app(app)
lm = LoginManager(app)
lm.login_view = 'login'
sio = SocketIO(app, async_mode='threading')  # threading mode for gunicorn

@lm.user_loader
def load_user(uid):
    return User.query.get(int(uid))

# creates db tables and adds default admin if none exists
with app.app_context():
    db.create_all()
    if not User.query.first():
        db.session.add(User(name='admin', pw=generate_password_hash('admin123')))
        db.session.commit()

# in-memory store for latest jpeg frame per esp_id
frames = {}

# shows login form and checks credentials on submit
@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        u = User.query.filter_by(name=request.form['username']).first()
        if u and check_password_hash(u.pw, request.form['password']):
            login_user(u)
            return redirect('/')
        return render_template('login.html', error='Wrong username or password')
    return render_template('login.html')

# logs the admin out and redirects to login
@app.route('/logout')
@login_required
def logout():
    logout_user()
    return redirect('/login')

# loads all scales and their unacked alert counts for home page
@app.route('/')
@login_required
def home():
    all_scales = Scale.query.all()
    sc_id = request.args.get('sc', type=int)
    selected = Scale.query.get(sc_id) if sc_id else (all_scales[0] if all_scales else None)
    scales_data = [(sc, Alert.query.filter_by(sc_id=sc.id, acked=False).count())
                   for sc in all_scales]
    recent_alerts = (Alert.query.join(Scale)
                     .order_by(Alert.time.desc()).limit(10).all())
    total_alerts = Alert.query.count()
    total_unacked = Alert.query.filter_by(acked=False).count()
    return render_template('home.html',
                           all_scales=all_scales,
                           selected=selected,
                           scales_data=scales_data,
                           recent_alerts=recent_alerts,
                           total_alerts=total_alerts,
                           total_unacked=total_unacked)


# adds a new scale, rejects duplicate esp_id
@app.route('/add', methods=['POST'])
@login_required
def add():
    if Scale.query.filter_by(esp_id=request.form['esp_id']).first():
        flash('ESP ID already exists', 'error')
        return redirect('/')
    db.session.add(Scale(name=request.form['name'],
                         location=request.form['location'],
                         esp_id=request.form['esp_id']))
    db.session.commit()
    return redirect('/')

# updates name, location and esp_id for an existing scale
@app.route('/edit/<int:id>', methods=['POST'])
@login_required
def edit(id):
    sc = Scale.query.get_or_404(id)
    sc.name = request.form['name']
    sc.location = request.form['location']
    sc.esp_id = request.form['esp_id']
    db.session.commit()
    return redirect('/')

# removes a scale, its alerts and its cached frame
@app.route('/delete/<int:id>', methods=['POST'])
@login_required
def delete(id):
    sc = Scale.query.get_or_404(id)
    frames.pop(sc.esp_id, None)
    db.session.delete(sc)
    db.session.commit()
    return redirect('/')

# shows a single scale's live video and last 50 alerts
@app.route('/view/<int:id>')
@login_required
def view(id):
    sc = Scale.query.get_or_404(id)
    alerts = Alert.query.filter_by(sc_id=sc.id).order_by(Alert.time.desc()).limit(50).all()
    return render_template('view.html', sc=sc, alerts=alerts)

# marks one alert as acknowledged and redirects back
@app.route('/ack/<int:id>', methods=['POST'])
@login_required
def ack(id):
    al = Alert.query.get_or_404(id)
    al.acked = True
    db.session.commit()
    ref = request.form.get('ref', '/')
    return redirect(ref)

# receives tamper alert from esp, saves it and notifies browser
@app.route('/alert', methods=['POST'])
def alert():
    data = request.get_json()
    sc = Scale.query.filter_by(esp_id=data.get('esp_id')).first()
    if not sc:
        return jsonify({'error': 'unknown esp_id'}), 404
    al = Alert(sc_id=sc.id)
    db.session.add(al)
    db.session.commit()
    sio.emit('alert', {'sc_id': sc.id, 'name': sc.name})
    return jsonify({'ok': True})

# stores the latest jpeg frame for a scale in memory
@app.route('/frame/<esp_id>', methods=['POST'])
def frame(esp_id):
    frames[esp_id] = request.data
    return jsonify({'ok': True})

# streams the latest frame as an mjpeg feed to the browser
@app.route('/video/<esp_id>')
@login_required
def video(esp_id):
    def gen():
        while True:
            if esp_id in frames:
                yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n'
                       + frames[esp_id] + b'\r\n')
            time.sleep(0.2)
    return Response(gen(), mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == '__main__':
    # starts the server with socketio
    sio.run(app, host='0.0.0.0', port=5000, allow_unsafe_werkzeug=True)
