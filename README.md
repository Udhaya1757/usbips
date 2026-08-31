# USBIPS — USB Intrusion Prevention System for Windows

USBIPS is a modular endpoint security solution designed to detect, classify, log, and prevent unauthorized or malicious USB device activity on Windows workstations.

---

## 🏛 Architecture Overview

USBIPS follows a client-server architecture:

```
+-------------------------------------------------------------+
|                     Windows Endpoint                        |
|                                                             |
|  +-------------------+        +--------------------------+  |
|  |  USB Monitor /    | -----> | Device Info Extractor &  |  |
|  |  WNDCLASS / GUID  |        | Classifier (HID/Mass/..) |  |
|  +-------------------+        +--------------------------+  |
|                                            |                |
|                                            v                |
|  +-------------------+        +--------------------------+  |
|  | Local SQLite DB & | <----> | Access Controller &      |  |
|  | Allowlist Engine  |        | SetupAPI Enforcer        |  |
|  +-------------------+        +--------------------------+  |
|                                            |                |
|                                            v                |
|  +-------------------------------------------------------+  |
|  | C++ REST Client (libcurl / WinHTTP)                   |  |
|  +-------------------------------------------------------+  |
+------------------------------|------------------------------+
                               | HTTPS / REST
                               v
+-------------------------------------------------------------+
|                      USBIPS Server                          |
|                                                             |
|  +-------------------------------------------------------+  |
|  | FastAPI REST API (Auth, Logs, Allowlist, Heartbeat)   |  |
|  +-------------------------------------------------------+  |
|                               |                             |
|                               v                             |
|  +-------------------------------------------------------+  |
|  | SQLAlchemy ORM & Database Layer (PostgreSQL / SQLite) |  |
|  +-------------------------------------------------------+  |
+-------------------------------------------------------------+
```

- **Client (C++ / Windows Console & Service)**:
  - Intercepts USB insertion/removal events via Windows message loops and device interface notifications.
  - Queries device hardware IDs (`VID`, `PID`, Serial Number, Class GUID) using Windows SetupAPI and `CfgMgr32`.
  - Classifies device capabilities (HID, Mass Storage, Network, BadUSB/Keystroke injection indicators).
  - Enforces allowlist/blocklist policies locally with low-latency SQLite lookups.
  - Controls device access via Windows device management APIs.
  - Streams security events and heartbeats to the central server.

- **Server (Python / FastAPI)**:
  - Centralized policy management and allowlist synchronization.
  - Real-time event ingestion and audit logging.
  - Client registration, token authentication, and health heartbeat monitoring.

---

## 📁 Repository Structure

```
USBIPS/
├── client/
│   └── src/
│       ├── usb/          # USB notification listeners & window message pumps
│       ├── device/       # Device info extraction (VID, PID, Serial, SetupAPI)
│       ├── classifier/   # Device type heuristics and BadUSB detection
│       ├── access/       # Device enable/disable & policy enforcement
│       ├── database/     # Client-side SQLite allowlist cache & local queues
│       ├── logging/      # Local audit file and console logger
│       ├── network/      # HTTP/REST client for server communication
│       └── service/      # Windows Service host & lifecycle management
├── server/
│   └── app/
│       ├── api/          # FastAPI route controllers (endpoints)
│       ├── models/       # Pydantic schemas & SQLAlchemy ORM models
│       ├── services/     # Business logic, rule engine, log processors
│       └── database/     # Database session management & migrations
├── docs/                 # Documentation, specifications, and architecture diagrams
└── README.md             # Project overview and developer setup
```

---

## 🚀 Getting Started

### Prerequisites
- **Client**: Visual Studio 2019/2022 with C++ Desktop Development tools (C++17 or higher) and Windows SDK.
- **Server**: Python 3.10+ and `pip`.

### Server Setup
1. Navigate to the server directory:
   ```bash
   cd server
   ```
2. Create and activate a virtual environment:
   ```bash
   python -m venv venv
   # On Windows:
   .\venv\Scripts\activate
   ```
3. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```
4. Run the FastAPI development server:
   ```bash
   uvicorn app.main:app --reload --host 127.0.0.1 --port 8000
   ```
5. Check API docs at `http://127.0.0.1:8000/docs`.

---

## 📄 License
Internal security project. All rights reserved.
