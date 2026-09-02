from __future__ import annotations

from typing import Optional

from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy import or_
from sqlalchemy.orm import Session

from app.database.database import get_db
from app.models.models import AllowlistDevice
from app.api.schemas import DeviceCheckRequest, DeviceCheckResponse, DeviceRecordResponse

router = APIRouter(prefix="/api", tags=["devices"])


@router.get("/devices", response_model=list[DeviceRecordResponse])
def list_devices(
    db: Session = Depends(get_db),
    policy: Optional[str] = Query(default=None, description="Optional filter for policy value such as ALLOWED or BLOCKED"),
):
    query = db.query(AllowlistDevice)
    if policy is not None:
        query = query.filter(AllowlistDevice.policy == policy.upper())

    devices = query.order_by(AllowlistDevice.id).all()
    return devices


@router.post("/devices/check", response_model=DeviceCheckResponse)
def check_device(payload: DeviceCheckRequest, db: Session = Depends(get_db)):
    if not payload.vid or not payload.pid:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail="vid and pid are required",
        )

    vid = payload.vid.strip().upper()
    pid = payload.pid.strip().upper()

    query = db.query(AllowlistDevice).filter(
        AllowlistDevice.vid == vid,
        AllowlistDevice.pid == pid,
    )

    if payload.serial:
        query = query.filter(
            or_(
                AllowlistDevice.serial.is_(None),
                AllowlistDevice.serial == "",
                AllowlistDevice.serial == payload.serial.strip(),
            )
        )

    match = query.order_by(AllowlistDevice.id).first()

    if not match:
        return DeviceCheckResponse(
            allowed=False,
            policy="BLOCKED",
            reason="NO_MATCH",
            device=None,
        )

    response_device = DeviceRecordResponse.model_validate(match)

    if match.policy.upper() == "ALLOWED":
        return DeviceCheckResponse(
            allowed=True,
            policy="ALLOWED",
            reason="ALLOWLIST_MATCH",
            device=response_device,
        )

    return DeviceCheckResponse(
        allowed=False,
        policy="BLOCKED",
        reason="DEVICE_BLOCKED",
        device=response_device,
    )
