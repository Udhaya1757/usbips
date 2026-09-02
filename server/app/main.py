# pyrefly: ignore [missing-import]
from fastapi import FastAPI

from app.api.clients import router as clients_router
from app.api.devices import router as devices_router
from app.api.events import router as events_router
from app.database.init_db import init_db

app = FastAPI(
    title="USBIPS Server",
    description="USB Intrusion Prevention System Central Management Server",
    version="1.0.0"
)


@app.on_event("startup")
def startup_event() -> None:
    init_db()


app.include_router(devices_router)
app.include_router(clients_router)
app.include_router(events_router)


@app.get("/")
def read_root():
    return {"message": "Hello USBIPS"}


@app.get("/health")
def health_check():
    return {"status": "ok", "server": "USBIPS"}
