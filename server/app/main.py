# pyrefly: ignore [missing-import]
from fastapi import FastAPI

app = FastAPI(
    title="USBIPS Server",
    description="USB Intrusion Prevention System Central Management Server",
    version="1.0.0"
)


@app.get("/")
def read_root():
    return {"message": "Hello USBIPS"}
