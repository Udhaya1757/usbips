from datetime import datetime

from sqlalchemy import DateTime, Index, String, Text
from sqlalchemy.orm import Mapped, mapped_column

from app.database.database import Base


class Client(Base):
    __tablename__ = "clients"

    id: Mapped[int] = mapped_column(primary_key=True, index=True)
    client_id: Mapped[str] = mapped_column(String(255), unique=True, index=True, nullable=False)
    hostname: Mapped[str | None] = mapped_column(String(255), nullable=True)
    ip_address: Mapped[str | None] = mapped_column(String(64), nullable=True)
    os: Mapped[str | None] = mapped_column(String(100), nullable=True)
    version: Mapped[str | None] = mapped_column(String(50), nullable=True)
    last_seen: Mapped[datetime | None] = mapped_column(DateTime, nullable=True, default=datetime.utcnow)
    status: Mapped[str] = mapped_column(String(20), nullable=False, default="OFFLINE")
    created_at: Mapped[datetime] = mapped_column(DateTime, nullable=False, default=datetime.utcnow)


class AllowlistDevice(Base):
    __tablename__ = "allowlist_devices"

    id: Mapped[int] = mapped_column(primary_key=True, index=True)
    vid: Mapped[str] = mapped_column(String(32), nullable=False, index=True)
    pid: Mapped[str] = mapped_column(String(32), nullable=False, index=True)
    serial: Mapped[str | None] = mapped_column(String(255), nullable=True)
    device_name: Mapped[str | None] = mapped_column(String(255), nullable=True)
    device_type: Mapped[str | None] = mapped_column(String(50), nullable=True)
    policy: Mapped[str] = mapped_column(String(20), nullable=False, default="ALLOWED")
    created_at: Mapped[datetime] = mapped_column(DateTime, nullable=False, default=datetime.utcnow)

    __table_args__ = (
        Index("ix_allowlist_vid_pid", "vid", "pid"),
    )


class SecurityEvent(Base):
    __tablename__ = "security_events"

    id: Mapped[int] = mapped_column(primary_key=True, index=True)
    client_id: Mapped[str | None] = mapped_column(String(255), nullable=True, index=True)
    event_type: Mapped[str | None] = mapped_column(String(100), nullable=True)
    severity: Mapped[str] = mapped_column(String(20), nullable=False, default="INFO")
    vid: Mapped[str | None] = mapped_column(String(32), nullable=True, index=True)
    pid: Mapped[str | None] = mapped_column(String(32), nullable=True, index=True)
    serial: Mapped[str | None] = mapped_column(String(255), nullable=True)
    device_name: Mapped[str | None] = mapped_column(String(255), nullable=True)
    message: Mapped[str | None] = mapped_column(Text, nullable=True)
    timestamp: Mapped[datetime] = mapped_column(DateTime, nullable=False, default=datetime.utcnow, index=True)

    __table_args__ = (
        Index("ix_security_event_client_timestamp", "client_id", "timestamp"),
    )
