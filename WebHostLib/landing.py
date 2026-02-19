from datetime import timedelta, datetime

from flask import render_template
from pony.orm import count

from WebHostLib import app, cache
from .models import Room, Seed


@app.route('/', methods=['GET', 'POST'])
@cache.cached(timeout=300)  # cache has to appear under app route for caching to work
def landing():
    rooms = count(room for room in Room if room.creation_time >= datetime.utcnow() - timedelta(days=7))
    seeds = count(seed for seed in Seed if seed.creation_time >= datetime.utcnow() - timedelta(days=7))
    return render_template("landing.html", rooms=rooms, seeds=seeds)


@app.route('/rooms', methods=['GET'])
def room_list():
    return render_template("roomList.html")


@app.route('/boot', methods=['GET'])
def boot_screen():
    return render_template("boot.html")


@app.route('/downloads', methods=['GET'])
def downloads_page():
    return render_template("downloads.html")


@app.route('/docs', methods=['GET'])
def docs_page():
    return render_template("docs.html")
