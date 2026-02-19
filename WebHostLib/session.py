from uuid import uuid4, UUID

from flask import session, render_template
from pony.orm import db_session

from WebHostLib import app
from WebHostLib.models import User


@app.before_request
def register_session():
    session.permanent = True  # technically 31 days after the last visit
    if not session.get("_id", None):
        session["_id"] = uuid4()  # uniquely identify each session without needing a login
    if session.get("discord_user") and "terms_accepted" not in session:
        discord_id = session["discord_user"].get("id")
        if discord_id:
            with db_session:
                user = User.get(discord_id=discord_id)
                current_version = app.config.get("TERMS_VERSION", "v1")
                if user:
                    session["terms_version"] = user.terms_version
                    session["terms_accepted"] = bool(user.terms_accepted and user.terms_version == current_version)
                else:
                    session["terms_accepted"] = False


@app.route('/session')
def show_session():
    return render_template(
        "session.html",
    )


@app.route('/session/<string:_id>')
def set_session(_id: str):
    new_id: UUID = UUID(_id, version=4)
    old_id: UUID = session["_id"]
    if old_id != new_id:
        session["_id"] = new_id
    return render_template(
        "session.html",
        old_id=old_id,
    )
