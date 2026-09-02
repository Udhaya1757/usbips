from __future__ import annotations

from datetime import datetime

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session

from app.api.schemas import SecurityEventCreate, SecurityEventResponse
from app.database.database import get_db
from app.models.models import SecurityEvent

router = APIRouter(prefix="/api", tags=["events"])


@router.post("/events", response_model=SecurityEventResponse)
def create_event(payload: SecurityEventCreate, db: Session = Depends(get_db)):
    if payload.client_id is None and payload.event_type is None and payload.message is None:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail="At least one of client_id, event_type, or message must be provided",
        )

    event_data = payload.model_dump(exclude_none=True)
    event_data["timestamp"] = payload.timestamp or datetime.utcnow()
    if "severity" not in event_data:
        event_data["severity"] = "INFO"

    event = SecurityEvent(**event_data)
    db.add(event)
    db.commit()
    db.refresh(event)
    return event
