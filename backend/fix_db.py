from main import app, db
with app.app_context():
    db.create_all()
    print("✅ SUCCESS : Les tables users et yaml_files sont creees !")
