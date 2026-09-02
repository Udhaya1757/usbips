from __future__ import annotations

from datetime import datetime
from typing import Optional

from pydantic import BaseModel, ConfigDict, Field


class DeviceCheckRequest(BaseModel):
    vid: str = Field(..., min_length=1, description="USB vendor ID", examples=["0951"])
    pid: str = Field(..., min_length=1, description="USB product ID", examples=["1666"])
    serial: Optional[str] = Field(default=None, description="Optional USB serial number", examples=["ABC123"])
    device_type: Optional[str] = Field(default=None, description="Optional device type", examples=["STORAGE"])


class DeviceRecordResponse(BaseModel):
    id: int
    vid: str
    pid: str
    serial: Optional[str] = None
    device_name: Optional[str] = None
    device_type: Optional[str] = None
    policy: str
    created_at: datetime

    model_config = ConfigDict(from_attributes=True)


class DeviceCheckResponse(BaseModel):
    allowed: bool
    policy: str
    reason: str
    device: Optional[DeviceRecordResponse] = None


class ClientRegisterRequest(BaseModel):
    client_id: str = Field(..., min_length=1, description="Unique USBIPS client identifier", examples=["USBIPS-TEST-CLIENT-001"])
    hostname: Optional[str] = Field(default=None, description="Computer hostname", examples=["DESKTOP-TEST"])
    ip_address: Optional[str] = Field(default=None, description="Current client IP address", examples=["192.168.1.10"])
    os: Optional[str] = Field(default=None, description="Operating system name", examples=["Windows 11"])
    version: Optional[str] = Field(default=None, description="USBIPS client version", examples=["1.0.0"])


class ClientResponse(BaseModel):
    id: int
    client_id: str
    hostname: Optional[str] = None
    ip_address: Optional[str] = None
    os: Optional[str] = None
    version: Optional[str] = None
    last_seen: Optional[datetime] = None
    status: str
    created_at: datetime

    model_config = ConfigDict(from_attributes=True)


class ClientHeartbeatRequest(BaseModel):
    client_id: str = Field(..., min_length=1, description="Client identifier to heartbeat for", examples=["USBIPS-TEST-CLIENT-001"])
    status: str = Field(default="ONLINE", description="Current status for the heartbeat", examples=["ONLINE"])


class HeartbeatResponse(BaseModel):
    client_id: str
    status: str
    last_seen: datetime
    message: str


class SecurityEventCreate(BaseModel):
    client_id: Optional[str] = Field(default=None, description="Client identifier associated with the event", examples=["USBIPS-TEST-CLIENT-001"])
    event_type: Optional[str] = Field(default=None, description="Security event type", examples=["USB_DEVICE_CONNECTED"])
    severity: str = Field(default="INFO", description="Event severity", examples=["INFO"])
    vid: Optional[str] = Field(default=None, description="USB vendor ID", examples=["0951"])
    pid: Optional[str] = Field(default=None, description="USB product ID", examples=["1666"])
    serial: Optional[str] = Field(default=None, description="USB serial number", examples=["ABC123"])
    device_name: Optional[str] = Field(default=None, description="Friendly device name", examples=["Kingston DataTraveler"])
    message: Optional[str] = Field(default=None, description="Event message", examples=["USB device connected and allowed"])
    timestamp: Optional[datetime] = Field(default=None, description="Timestamp for the event")


class SecurityEventResponse(BaseModel):
    id: int
    client_id: Optional[str] = None
    event_type: Optional[str] = None
    severity: str
    vid: Optional[str] = None
    pid: Optional[str] = None
    serial: Optional[str] = None
    device_name: Optional[str] = None
    message: Optional[str] = None
    timestamp: datetime

    model_config = ConfigDict(from_attributes=True)
