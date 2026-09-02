from sqlalchemy.orm import Session

from app.database.database import Base, SessionLocal, engine
from app.models.models import AllowlistDevice


def create_tables() -> None:
    Base.metadata.create_all(bind=engine)


def seed_allowlist_devices(db: Session) -> None:
    seed_data = [
        {
            "vid": "0951",
            "pid": "1666",
            "serial": None,
            "device_name": "Kingston",
            "device_type": "STORAGE",
            "policy": "ALLOWED",
        },
        {
            "vid": "046D",
            "pid": "C52B",
            "serial": None,
            "device_name": "Logitech",
            "device_type": "HID",
            "policy": "ALLOWED",
        },
        {
            "vid": "TEST",
            "pid": "BAD1",
            "serial": None,
            "device_name": "TEST/BAD",
            "device_type": "OTHER",
            "policy": "BLOCKED",
        },
    ]

    for entry in seed_data:
        existing = (
            db.query(AllowlistDevice)
            .filter(AllowlistDevice.vid == entry["vid"], AllowlistDevice.pid == entry["pid"])
            .first()
        )
        if existing:
            continue

        db.add(AllowlistDevice(**entry))

    db.commit()


def init_db() -> None:
    create_tables()
    db = SessionLocal()
    try:
        seed_allowlist_devices(db)
    finally:
        db.close()
