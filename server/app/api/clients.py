from __future__ import annotations

from datetime import datetime

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session

from app.api.schemas import ClientHeartbeatRequest, ClientRegisterRequest, ClientResponse, HeartbeatResponse
from app.database.database import get_db
from app.models.models import Client

router = APIRouter(prefix="/api", tags=["clients"])


@router.post("/clients/register", response_model=ClientResponse)
def register_client(payload: ClientRegisterRequest, db: Session = Depends(get_db)):
    if not payload.client_id or not payload.client_id.strip():
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail="client_id is required",
        )

    client = db.query(Client).filter(Client.client_id == payload.client_id.strip()).first()
    now = datetime.utcnow()

    if client is None:
        client = Client(
            client_id=payload.client_id.strip(),
            hostname=payload.hostname,
            ip_address=payload.ip_address,
            os=payload.os,
            version=payload.version,
            last_seen=now,
            status="ONLINE",
        )
        db.add(client)
    else:
        if payload.hostname is not None:
            client.hostname = payload.hostname
        if payload.ip_address is not None:
            client.ip_address = payload.ip_address
        if payload.os is not None:
            client.os = payload.os
        if payload.version is not None:
            client.version = payload.version
        client.last_seen = now
        client.status = "ONLINE"

    db.commit()
    db.refresh(client)
    return client


@router.post("/clients/heartbeat", response_model=HeartbeatResponse)
def client_heartbeat(payload: ClientHeartbeatRequest, db: Session = Depends(get_db)):
    client = db.query(Client).filter(Client.client_id == payload.client_id.strip()).first()
    if client is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Client not found",
        )

    client.last_seen = datetime.utcnow()
    client.status = payload.status.strip().upper() if payload.status else "ONLINE"
    db.commit()
    db.refresh(client)

    return HeartbeatResponse(
        client_id=client.client_id,
        status=client.status,
        last_seen=client.last_seen,
        message="heartbeat acknowledged",
    )
