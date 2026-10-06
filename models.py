from flask_sqlalchemy import SQLAlchemy
from flask_login import UserMixin
from datetime import datetime

db = SQLAlchemy()

# stores the single admin account
class User(UserMixin, db.Model):
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(64), unique=True, nullable=False)
    pw = db.Column(db.String(128), nullable=False)

# represents one physical scale with its esp id
class Scale(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(64), nullable=False)
    location = db.Column(db.String(128), nullable=False)
    esp_id = db.Column(db.String(64), unique=True, nullable=False)
    alerts = db.relationship('Alert', backref='scale', cascade='all, delete-orphan')

# records each tamper event; acked turns true when dismissed
class Alert(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    sc_id = db.Column(db.Integer, db.ForeignKey('scale.id'), nullable=False)
    time = db.Column(db.DateTime, default=datetime.utcnow)
    acked = db.Column(db.Boolean, default=False)
