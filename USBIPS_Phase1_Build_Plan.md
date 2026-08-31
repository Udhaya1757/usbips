# USBIPS Phase 1 — Complete Build Plan & To-Do List
### For Antigravity IDE Agent Mode

---

## Where You Stand Right Now (25% Done)

| Status | Item |
|--------|------|
| ✅ Done | Project architecture designed |
| ✅ Done | Technology stack decided (C++ client + Python/FastAPI server) |
| ✅ Done | Phase 1 development plan written |
| ✅ Done | Visual Studio project setup guidance received |
| ✅ Done | Phase 1A USB Monitor code received from ChatGPT |
| ✅ Done | `CDevice`/`USBDevice` structure understood |
| 🔲 Pending | Everything below this line |

**The remaining 75% is 12 concrete tasks. Each task below is one session with Antigravity.**

---

## How to Use This Plan with Antigravity

Each task has three parts:
1. **Your Prompt** — exactly what you type into Antigravity agent chat
2. **Antigravity Will Explain** — what the agent narrates while it builds, so you learn
3. **You Test** — how you verify it works before moving on

**Rule: Never start the next task until the current one's test passes.**

---

## Dependency Chain

```
Task 0 (Setup)
    ↓
Task 1 (USB Monitor — Phase 1A)
    ↓
Task 2 (Device Info Extractor — Phase 1B)
    ↓
Task 3 (Classifier — Phase 1C)
    ↓
Task 4 (SQLite Allowlist — Phase 1D)
    ↓
Task 5 (Access Controller — Phase 1E)
    ↓
Task 6 (Client Event Logger — Phase 1F)
    ↓
Task 7 (FastAPI Server — Phase 1G)
    ↓
Task 8 (Server APIs — Phase 1H)
    ↓
Task 9 (REST Client + C↔S Integration — Phase 1I)
    ↓
Task 10 (Heartbeat & Client Manager — Phase 1J)
    ↓
Task 11 (Windows Service — Phase 1K)
    ↓
Task 12 (Integration Test — Phase 1L)
```

---

---

# TASK 0 — Dev Environment Setup

**Goal:** Both the C++ client project and the Python server project build and run from scratch.

## Your Prompt to Antigravity

```
I am building a project called USBIPS — a USB Intrusion Prevention System for Windows.
The client is a C++ console application in Visual Studio.
The server is a Python FastAPI application.

Set up the folder structure for this project:

USBIPS/
├── client/
│   └── src/
│       ├── usb/
│       ├── device/
│       ├── classifier/
│       ├── access/
│       ├── database/
│       ├── logging/
│       ├── network/
│       └── service/
├── server/
│   └── app/
│       ├── api/
│       ├── models/
│       ├── services/
│       └── database/
├── docs/
└── README.md

Then:
1. Create a README.md with project description.
2. Create a server/requirements.txt listing: fastapi, uvicorn, sqlalchemy, pydantic, python-multipart
3. Create server/app/main.py with a basic FastAPI "Hello USBIPS" endpoint at GET /
4. Create a .gitignore for both C++ and Python.

Explain each file as you create it.
```

## Antigravity Will Explain While Building
- "Creating the folder structure — this separates client (C++) from server (Python) so both can grow independently"
- "requirements.txt lists all Python libraries we need — uvicorn runs the server, FastAPI defines the API, SQLAlchemy talks to SQLite"
- "main.py is the entry point for the FastAPI server — right now it just says Hello so we can verify the server starts"
- "The .gitignore prevents compiled files (.exe, .obj, __pycache__) from polluting your Git repository"

## You Test
```bash
# In terminal, go to server/ folder:
pip install -r requirements.txt
uvicorn app.main:app --reload

# Open browser: http://127.0.0.1:8000
# Should see: {"message": "Hello USBIPS"}
# Also visit: http://127.0.0.1:8000/docs  ← auto-generated API docs
```

**✅ Task 0 done when: server starts and browser shows the hello response.**

---

---

# TASK 1 — USB Device Monitor (Phase 1A)

**Goal:** C++ program that prints a message every time a USB device is plugged in or removed.

## Your Prompt to Antigravity

```
I am building USBIPS, a Windows USB security system in C++.

Create a file: client/src/usb/USBMonitor.cpp

This file should:
1. Register for Windows USB device notifications using RegisterDeviceNotificationW()
   with GUID_DEVINTERFACE_USB_DEVICE, GUID_DEVINTERFACE_HID, and GUID_DEVINTERFACE_VOLUME
2. Run a hidden message loop using CreateWindowExW() — we need a window handle to receive
   WM_DEVICECHANGE messages from Windows
3. When a USB arrives (DBT_DEVICEARRIVAL), print:
   ========================================
   USB DEVICE CONNECTED
   Device Interface: <path>
   ========================================
4. When a USB is removed (DBT_DEVICEREMOVECOMPLETE), print:
   ========================================
   USB DEVICE REMOVED
   ========================================
5. Have a clean shutdown on Ctrl+C using SetConsoleCtrlHandler()

Also create: client/src/usb/USBMonitor.h
with the class definition for USBMonitor.

Create: client/src/main.cpp
that creates a USBMonitor object and calls monitor.Start()

Explain every Windows API call and why we need it as you write the code.
```

## Antigravity Will Explain While Building
- "RegisterDeviceNotificationW tells Windows: 'send me a message when a USB device connects' — without this Windows won't tell our program about USB events"
- "We create a hidden window because WM_DEVICECHANGE is a Windows message — only windows can receive messages in Windows. We make it invisible because we're a security daemon, not a visible app"
- "DBT_DEVICEARRIVAL is the event code for 'device arrived' — DBT_DEVICEREMOVECOMPLETE means it was unplugged"
- "The device interface path looks like \\\\?\\USB#VID_0951&PID_1666#... — this string identifies the exact device and we'll use it in the next task to extract more info"
- "SetConsoleCtrlHandler lets us do clean shutdown when Ctrl+C is pressed — important for a security service"

## You Test
```
1. Build in Visual Studio (Ctrl+Shift+B)
2. Run the .exe as Administrator
3. Plug in a USB flash drive
4. Should see "USB DEVICE CONNECTED" in the console
5. Unplug it
6. Should see "USB DEVICE REMOVED"
7. Test with: keyboard, mouse, flash drive
```

**✅ Task 1 done when: every USB plug/unplug shows a message in the console.**

---

---

# TASK 2 — Device Information Extractor (Phase 1B)

**Goal:** For every USB that connects, extract and print VID, PID, Serial Number, device description, and drive letter.

## Your Prompt to Antigravity

```
I am building USBIPS. I already have a USB monitor that detects plug/unplug events.
Now I need to extract device information.

Create: client/src/device/USBDevice.h
This is the data structure to hold device information:

struct USBDevice {
    std::wstring devicePath;     // raw Windows device interface path
    std::wstring vid;            // Vendor ID, e.g. "0951"
    std::wstring pid;            // Product ID, e.g. "1666"
    std::wstring serialNumber;   // device serial number
    std::wstring deviceType;     // "HID", "STORAGE", "NETWORK", "OTHER"
    std::wstring description;    // e.g. "Kingston DataTraveler"
    std::wstring driveLetter;    // e.g. "E:" — only for storage devices
    std::wstring volumeSerial;   // volume serial number of the drive
    std::wstring deviceClass;    // Windows device class GUID as string
    FILETIME firstSeen;
    FILETIME lastSeen;
};

Create: client/src/device/DeviceInfoExtractor.h and DeviceInfoExtractor.cpp

The extractor should take a device interface path (from the USB monitor event) and:
1. Parse VID and PID from the device path string (they appear as VID_xxxx&PID_xxxx)
2. Use SetupAPI (SetupDiGetDeviceRegistryProperty) to get:
   - SPDRP_FRIENDLYNAME for description
   - SPDRP_CLASSGUID for device class
3. Use CM_Get_Device_ID to get the full hardware ID and serial number
4. For STORAGE devices: use GetVolumeInformation to get drive letter and volume serial
5. Return a filled USBDevice struct

Link against: setupapi.lib

Explain each Windows API and why it's needed, and why each field matters for security.
```

## Antigravity Will Explain While Building
- "SetupAPI is the Windows library for enumerating and querying hardware devices — it's how Windows Device Manager reads device info"
- "VID (Vendor ID) identifies the manufacturer — 0951 is Kingston, 046D is Logitech. PID (Product ID) identifies the specific product model"
- "Serial Number is what makes a specific device unique — two identical Kingston flash drives have the same VID/PID but different serials. This is critical for our allowlist"
- "SPDRP_FRIENDLYNAME gives the human-readable name Windows shows — e.g. 'Kingston DataTraveler 3.0'"
- "For storage devices we call GetVolumeInformation to find which drive letter (E:, F:) Windows assigned — we need this to monitor file activity later"
- "We're filling a USBDevice struct — think of it as the 'identity card' for every USB device. Everything we do in USBIPS later (allowlist lookup, logging, server sync) uses this struct"

## You Test
```
Plug in a USB flash drive.
Console should print something like:

===== USB DEVICE INFORMATION =====
VID         : 0951
PID         : 1666
Serial      : 00000000000000000D
Description : Kingston DataTraveler 3.0 USB Device
Device Path : \\?\USB#VID_0951&PID_1666#...
Drive       : E:
Volume SN   : A1B2C3D4
===================================

Try with: flash drive, keyboard, mouse, phone
```

**✅ Task 2 done when: real VID/PID/Serial/Description prints for every plugged device.**

---

---

# TASK 3 — Device Classifier (Phase 1C)

**Goal:** Automatically label every USB device as HID, STORAGE, NETWORK, or OTHER.

## Your Prompt to Antigravity

```
I am building USBIPS. I have a USBDevice struct with device info.
Now I need to classify devices by type.

Create: client/src/classifier/DeviceClassifier.h and DeviceClassifier.cpp

Rules for classification:
- If Windows device class GUID matches GUID_DEVCLASS_HIDCLASS → DeviceType = "HID"
- If Windows device class GUID matches GUID_DEVCLASS_DISKDRIVE or GUID_DEVCLASS_USB for storage → DeviceType = "STORAGE"
- If Windows device class GUID matches GUID_DEVCLASS_NET → DeviceType = "NETWORK"
- Fallback → DeviceType = "OTHER"

Also check:
- If devicePath contains "HID" → HID
- If device has a drive letter assigned → STORAGE  
- If description contains "Wi-Fi", "Wireless", "Ethernet", "Network" → NETWORK

The classifier should:
1. Take a USBDevice by reference
2. Set device.deviceType
3. Log which rule triggered the classification

Also define an enum in USBDevice.h:
enum class DeviceType { HID, STORAGE, NETWORK, OTHER };

Explain what each device class GUID represents and how Windows uses them internally.
```

## Antigravity Will Explain While Building
- "Windows assigns every device a class GUID — it's a unique ID that says what kind of hardware this is. GUID_DEVCLASS_HIDCLASS = Human Interface Device (keyboards, mice, gamepads)"
- "We check multiple signals because no single check is perfectly reliable — a device might have a drive letter but no storage class, or report the wrong class entirely. Layered checks are more robust"
- "The DeviceType enum in C++ is better than raw strings — it means the compiler will catch typos, and switch statements will warn you if you miss a case"
- "This classifier is the first line of defense — an 'OTHER' device is immediately suspicious and gets stricter treatment in our allowlist and logging"

## You Test
```
Connect each device and verify correct classification:

✅ Keyboard         → HID
✅ Mouse            → HID
✅ USB Flash Drive  → STORAGE
✅ Phone (charging) → OTHER or STORAGE depending on mode
✅ USB Wi-Fi adapter → NETWORK (if you have one)

Console should print:
[CLASSIFIER] Device type: STORAGE (rule: drive letter assigned)
```

**✅ Task 3 done when: all test devices classify correctly.**

---

---

# TASK 4 — Local SQLite Allowlist (Phase 1D)

**Goal:** Store trusted devices in a local database. Look up whether a plugged device is trusted.

## Your Prompt to Antigravity

```
I am building USBIPS. I need a local SQLite database to store trusted (allowlisted) USB devices.

Add SQLite to the C++ project. I will use the sqlite3 amalgamation (sqlite3.h + sqlite3.c).
Download it and add it to: client/src/database/

Create: client/src/database/LocalDatabase.h and LocalDatabase.cpp

This class manages a file: usbips_local.db

Create this table:

CREATE TABLE IF NOT EXISTS allowlist (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    vid             TEXT NOT NULL,
    pid             TEXT NOT NULL,
    serial_number   TEXT,
    device_type     TEXT NOT NULL,
    description     TEXT,
    volume_serial   TEXT,
    status          TEXT DEFAULT 'ALLOWED',
    friendly_name   TEXT,
    created_at      DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at      DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS events (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    event_type      TEXT NOT NULL,
    vid             TEXT,
    pid             TEXT,
    serial_number   TEXT,
    device_type     TEXT,
    decision        TEXT,
    reason          TEXT,
    timestamp       DATETIME DEFAULT CURRENT_TIMESTAMP,
    synced_to_server INTEGER DEFAULT 0
);

Methods to implement:
- bool Initialize(const std::string& dbPath)
- bool IsDeviceAllowed(const USBDevice& device)
- bool AddDevice(const USBDevice& device, const std::string& friendlyName)
- bool RemoveDevice(const std::string& vid, const std::string& pid, const std::string& serial)
- bool UpdateDeviceList(const std::vector<USBDevice>& serverList)  // for later sync
- bool LogEvent(const std::string& eventType, const USBDevice& device, const std::string& decision, const std::string& reason)
- std::vector<USBDevice> GetPendingEvents()  // events not yet synced to server

Matching rule: a device is ALLOWED if VID + PID match AND (serial is empty in allowlist OR serial matches).
Explain what each table column is for and why the matching rule is designed this way.
```

## Antigravity Will Explain While Building
- "SQLite is a file-based database — the entire database is one file (usbips_local.db). No server needed, no installation. It's perfect for an offline-capable client"
- "The allowlist table stores our trusted devices. Think of it as a whitelist of 'known good' hardware"
- "The matching rule: VID+PID identifies the model of device (e.g. 'Kingston DataTraveler 3.0'). Serial number identifies the specific unit. If the allowlist has an empty serial, any device of that model is trusted. If it has a serial, ONLY that exact physical device is trusted — much stricter"
- "The events table stores everything that happened — connects, disconnects, blocks. The synced_to_server=0 flag means 'not yet sent to server'. This lets the client work offline and batch-upload later"
- "UpdateDeviceList() will be used when the server pushes a fresh allowlist — the client will call this to refresh its local copy"

## You Test
```cpp
// In main.cpp, test code:
LocalDatabase db;
db.Initialize("usbips_local.db");

USBDevice testDevice;
testDevice.vid = "0951";
testDevice.pid = "1666";
testDevice.serialNumber = "TEST001";
testDevice.deviceType = "STORAGE";

// First: not in allowlist
std::cout << db.IsDeviceAllowed(testDevice);  // prints 0 (false)

// Add it
db.AddDevice(testDevice, "My Kingston Drive");

// Now allowed
std::cout << db.IsDeviceAllowed(testDevice);  // prints 1 (true)

// Inspect the database file with DB Browser for SQLite (free tool)
```

**✅ Task 4 done when: IsDeviceAllowed returns false for unknown, true after adding.**

---

---

# TASK 5 — Access Control Decision Engine (Phase 1E)

**Goal:** Connect Monitor → Extractor → Classifier → Allowlist into one pipeline that makes ALLOW / BLOCK / ASK decisions.

## Your Prompt to Antigravity

```
I am building USBIPS. I have:
- USBMonitor (detects plug/unplug)
- DeviceInfoExtractor (fills USBDevice struct)
- DeviceClassifier (sets device type)
- LocalDatabase (checks allowlist)

Now I need to wire them all together into an access control pipeline.

Create: client/src/access/AccessController.h and AccessController.cpp

enum class AccessDecision { ALLOW, BLOCK, ASK };

struct DecisionResult {
    AccessDecision decision;
    std::string reason;    // e.g. "ALLOWLIST_MATCH", "UNKNOWN_DEVICE", "USER_APPROVED"
};

class AccessController {
public:
    AccessController(LocalDatabase* db);
    DecisionResult EvaluateDevice(USBDevice& device);
    void HandleUserDecision(USBDevice& device, bool userApproved);
private:
    LocalDatabase* m_db;
};

Decision logic:
1. Extract device info and classify
2. Query local allowlist:
   - MATCH → return ALLOW with reason "ALLOWLIST_MATCH"
   - NO MATCH → return ASK with reason "UNKNOWN_DEVICE"
3. HandleUserDecision(): if approved → add to local allowlist → log USER_APPROVED
                         if rejected → log USER_REJECTED → return BLOCK

For now, simulate user input with std::cin (ask in console). 
We will replace this with a real UI prompt later.

In main.cpp, update the USB arrival handler to call AccessController::EvaluateDevice()
and print the full decision result.

Explain what "policy" means in security context and why we separate the decision engine
from the UI input.
```

## Antigravity Will Explain While Building
- "AccessController is the brain of USBIPS — every USB device passes through it. The monitor just detects. The extractor just reads. But the controller DECIDES"
- "We separate decision logic from UI intentionally — the controller doesn't know or care whether it's printing to console, showing a popup, or sending to a web dashboard. This is called 'separation of concerns' and it makes the code reusable when we add the Windows Service and real UI later"
- "The three decisions: ALLOW means 'trusted, proceed'. BLOCK means 'not allowed, refuse'. ASK means 'unknown, consult the user or the server'. ASK is the most important — it's what happens in the real world when someone plugs in a new device"
- "The 'reason' field tells us WHY the decision was made — ALLOWLIST_MATCH vs USER_APPROVED vs UNKNOWN_DEVICE. This matters for the audit log and the server dashboard"
- "When a user approves a device, we immediately add it to the local SQLite allowlist. Next time the same device is plugged in, it's ALLOWLIST_MATCH, not ASK"

## You Test
```
Scenario 1: Plug in a known USB (you've never added it)
→ Should print: DECISION: ASK — UNKNOWN_DEVICE
→ Type 'y' in console
→ Should print: USER_APPROVED — added to allowlist

Scenario 2: Unplug and replug the same USB
→ Should print: DECISION: ALLOW — ALLOWLIST_MATCH

Scenario 3: Plug in a different USB
→ Should print: DECISION: ASK — UNKNOWN_DEVICE
→ Type 'n'
→ Should print: USER_REJECTED — BLOCK
```

**✅ Task 5 done when: all three scenarios work correctly.**

---

---

# TASK 6 — Client Event Logger (Phase 1F)

**Goal:** Every important action is saved to the local database AND written to a log file.

## Your Prompt to Antigravity

```
I am building USBIPS. I need a proper event logger.

Create: client/src/logging/EventLogger.h and EventLogger.cpp

Event types (define as enum):
DEVICE_CONNECTED, DEVICE_REMOVED, ALLOWLIST_MATCH,
UNKNOWN_DEVICE, USER_APPROVED, USER_REJECTED, DEVICE_BLOCKED,
SERVER_SYNC_SUCCESS, SERVER_SYNC_FAILED

struct SecurityEvent {
    std::string eventId;        // UUID-like unique ID: timestamp + random suffix
    std::string eventType;      // from enum above, as string
    std::string clientId;       // unique ID of this machine (use MAC address or hostname+UUID)
    std::string timestamp;      // ISO 8601 format: 2026-08-28T21:10:00Z
    USBDevice   device;         // which device triggered this
    std::string decision;       // ALLOW / BLOCK / ASK
    std::string reason;         // e.g. ALLOWLIST_MATCH
    bool        syncedToServer; // has this been uploaded yet?
};

EventLogger class:
- Initialize(LocalDatabase* db, const std::string& logFilePath)
- void Log(SecurityEvent& event)  // writes to both file and database
- std::vector<SecurityEvent> GetUnsynced()  // returns events with syncedToServer=false
- void MarkSynced(const std::string& eventId)

Log file format (one line per event, pipe-separated):
2026-08-28T21:10:05Z | DEVICE_CONNECTED | STORAGE | VID:0951 PID:1666 | ALLOW | ALLOWLIST_MATCH

Generate a clientId on first run and persist it in a config file: usbips_client.conf
Format: client_id=USBIPS-<hostname>-<random-8-hex-chars>

Explain why each event needs a unique eventId, why we need both file logging AND database logging,
and what clientId is for.
```

## Antigravity Will Explain While Building
- "A unique eventId lets the server detect duplicate uploads — if the client tries to upload the same event twice (e.g. network retry), the server can ignore the duplicate by checking the eventId"
- "We log to both a file AND the database: the file is human-readable (you can open it in Notepad or grep it). The database is machine-queryable (the server can sync it, we can run SQL queries). Both serve different purposes"
- "clientId identifies which Windows machine this is — when multiple PCs run USBIPS and all report to the same server, the server needs to know 'this event happened on PC-FINANCE-01, not PC-RECEPTION'"
- "ISO 8601 timestamp (2026-08-28T21:10:05Z) is a universal time format — all systems understand it, and it sorts correctly alphabetically. Never use local time formats in logs"
- "syncedToServer=false is the 'outbox' flag — when the server is offline, events pile up in the local database. When it comes back, we send everything with syncedToServer=false, then set them to true"

## You Test
```
After plugging in a USB:
1. Check console for event printout
2. Open usbips_events.log — should have pipe-separated entries
3. Open usbips_local.db in DB Browser for SQLite
4. Check the events table — should have rows with synced_to_server=0
```

**✅ Task 6 done when: events appear in both the log file and the SQLite database.**

---

---

# TASK 7 — FastAPI Server Foundation (Phase 1G)

**Goal:** A running server with a database that can store clients, devices, and events.

## Your Prompt to Antigravity

```
I am building USBIPS. Now I need the central server.

In the server/ folder, build this:

1. server/app/database/database.py
   - SQLAlchemy engine connecting to usbips_server.db (SQLite)
   - Session factory
   - Base model class

2. server/app/models/models.py — SQLAlchemy ORM models:

   Client:
     id, client_id (unique string), hostname, ip_address,
     usbips_version, status (ONLINE/OFFLINE),
     last_heartbeat, registered_at

   AllowlistDevice:
     id, vid, pid, serial_number, device_type, description,
     friendly_name, status (ALLOWED/BLOCKED/REVOKED),
     created_at, updated_at, created_by

   SecurityEvent:
     id, event_id (unique), client_id (FK to Client),
     timestamp, event_type, vid, pid, serial_number,
     device_type, decision, reason, raw_event_json

3. server/app/database/init_db.py
   - Create all tables on startup
   - Insert 3 sample allowlist entries for testing:
     * VID:0951 PID:1666 — Kingston DataTraveler — ALLOWED
     * VID:046D PID:C52B — Logitech Receiver — ALLOWED
     * VID:TEST PID:BAD1 — Test Block Device — BLOCKED

4. Update server/app/main.py to call init_db on startup and add:
   GET /health → {"status": "ok", "server": "USBIPS", "version": "1.0"}

Explain what ORM means, what SQLAlchemy does, and why we use SQLite here.
```

## Antigravity Will Explain While Building
- "ORM stands for Object-Relational Mapping — it lets you work with database rows as Python objects instead of writing raw SQL. SQLAlchemy is the most popular ORM in Python"
- "Instead of writing 'INSERT INTO clients VALUES(...)' you just write client = Client(hostname='PC-01') and the ORM translates it to SQL for you"
- "We use SQLite here for the same reason as the client — it's a file, needs no installation, and is perfect for development. Later you can swap it for PostgreSQL by changing one line"
- "The Client model stores which Windows machines are running USBIPS. The AllowlistDevice is the master list of trusted hardware. SecurityEvent is the audit trail"
- "We pre-seed 3 test devices so you can immediately test the allowlist API without manually adding data"

## You Test
```bash
cd server/
uvicorn app.main:app --reload

# Test in browser:
http://127.0.0.1:8000/health
# → {"status": "ok", "server": "USBIPS", "version": "1.0"}

http://127.0.0.1:8000/docs
# → You should see FastAPI's interactive API docs

# Verify database was created:
ls -la usbips_server.db   # file should exist
# Open in DB Browser for SQLite and check the 3 pre-seeded devices
```

**✅ Task 7 done when: /health returns OK and the database has 3 devices pre-seeded.**

---

---

# TASK 8 — Server API Endpoints (Phase 1H)

**Goal:** The server can manage allowlist entries, register clients, accept events, and check device policy.

## Your Prompt to Antigravity

```
I am building USBIPS server with FastAPI. Build the complete Phase 1 REST API.

Create: server/app/api/ with these files:

--- devices.py (Allowlist Management API) ---
GET    /api/devices          → list all allowlist entries (support ?status=ALLOWED filter)
POST   /api/devices          → add a device to allowlist
GET    /api/devices/{id}     → get one device
PUT    /api/devices/{id}     → update device (e.g. change status to REVOKED)
DELETE /api/devices/{id}     → remove from allowlist
POST   /api/devices/check    → check if a device is allowed
  Request:  { "vid": "0951", "pid": "1666", "serial": "ABC123", "device_type": "STORAGE" }
  Response: { "allowed": true, "reason": "ALLOWLIST_MATCH", "device": {...} }

--- clients.py (Client Management API) ---
POST   /api/clients/register → register a new USBIPS client
  Request:  { "client_id": "USBIPS-PC01-ABCD1234", "hostname": "PC01", "ip": "192.168.1.10", "version": "1.0" }
  Response: { "registered": true, "client_id": "..." }
POST   /api/clients/heartbeat → client sends heartbeat
  Request:  { "client_id": "..." }
  Response: { "acknowledged": true }
GET    /api/clients          → list all registered clients

--- events.py (Event/Log API) ---
POST   /api/events           → client uploads a security event
GET    /api/events           → get all events (support ?client_id=... and ?limit=100 filters)
GET    /api/events/{event_id} → get one event by ID

Create Pydantic schemas in: server/app/models/schemas.py
(Request and response shapes for each endpoint)

Add error handling: 404 for not found, 422 for invalid input.
Explain what each endpoint does and why it exists.
```

## Antigravity Will Explain While Building
- "Each API endpoint is a URL the C++ client can call over HTTP. POST /api/devices/check is the most critical one — the client calls this when a USB is plugged in to ask 'is this device trusted?'"
- "Pydantic schemas define what JSON format the API accepts and returns. If the client sends wrong fields, Pydantic automatically rejects the request with a 422 error — no bad data gets into the database"
- "GET /api/clients lets the admin dashboard see all registered USBIPS clients and their status — this is how the admin knows which PCs are protected"
- "POST /api/clients/heartbeat is a 'I'm still alive' message — the client sends it every few minutes. If the server stops seeing heartbeats, it marks that client OFFLINE"
- "POST /api/events is how the client uploads its local event log to the server. The server stores them centrally so the admin can see all events from all clients in one place"

## You Test
```bash
# With server running, test using the /docs interface or curl:

# Check if a device is allowed:
curl -X POST http://localhost:8000/api/devices/check \
  -H "Content-Type: application/json" \
  -d '{"vid":"0951","pid":"1666","serial":"ABC","device_type":"STORAGE"}'
# → {"allowed": true, "reason": "ALLOWLIST_MATCH", ...}

# Check a blocked device:
curl -X POST http://localhost:8000/api/devices/check \
  -H "Content-Type: application/json" \
  -d '{"vid":"TEST","pid":"BAD1","serial":"","device_type":"OTHER"}'
# → {"allowed": false, "reason": "DEVICE_BLOCKED", ...}

# Register a client:
curl -X POST http://localhost:8000/api/clients/register \
  -H "Content-Type: application/json" \
  -d '{"client_id":"USBIPS-PC01-TEST0001","hostname":"PC01","ip":"127.0.0.1","version":"1.0"}'
# → {"registered": true}

# Check /docs for full interactive testing
```

**✅ Task 8 done when: all endpoints respond correctly in /docs.**

---

---

# TASK 9 — C++ REST Client + Client↔Server Integration (Phase 1I)

**Goal:** The C++ client contacts the server when a USB is plugged in and syncs events.

## Your Prompt to Antigravity

```
I am building USBIPS. The C++ client needs to make HTTP requests to the FastAPI server.

I will use libcurl for HTTP in C++. Add libcurl to the Visual Studio project.
(Use vcpkg to install: vcpkg install curl:x64-windows)

Create: client/src/network/ServerClient.h and ServerClient.cpp

class ServerClient {
public:
    ServerClient(const std::string& serverUrl, const std::string& clientId);

    // Check if a device is allowed (calls POST /api/devices/check)
    struct PolicyResponse {
        bool allowed;
        std::string reason;
    };
    PolicyResponse CheckDevicePolicy(const USBDevice& device);

    // Register this client with the server (calls POST /api/clients/register)
    bool RegisterClient(const std::string& hostname);

    // Send heartbeat (calls POST /api/clients/heartbeat)
    bool SendHeartbeat();

    // Upload a batch of unsynced events (calls POST /api/events for each)
    bool SyncEvents(const std::vector<SecurityEvent>& events);

    // Download latest allowlist from server and return it
    std::vector<USBDevice> FetchAllowlist();

private:
    std::string m_serverUrl;
    std::string m_clientId;

    // Helper: make HTTP POST with JSON body, return response JSON string
    std::string HttpPost(const std::string& endpoint, const std::string& jsonBody);

    // Helper: make HTTP GET, return response JSON string
    std::string HttpGet(const std::string& endpoint);
};

Use nlohmann/json for JSON parsing in C++.
(vcpkg install nlohmann-json:x64-windows)

Update AccessController to:
- First check local SQLite allowlist (fast, works offline)
- If not found locally, call ServerClient::CheckDevicePolicy() (requires network)
- If server is unreachable, fall back to local decision only and log SERVER_UNAVAILABLE

Add a background thread for heartbeat: every 60 seconds call ServerClient::SendHeartbeat()
Add event sync: after every decision, call ServerClient::SyncEvents() for unsynced events

Explain the offline-first design pattern and why local SQLite check comes BEFORE server check.
```

## Antigravity Will Explain While Building
- "libcurl is the standard C library for HTTP requests — the same library that the curl command-line tool uses. It handles all the HTTP protocol details for us"
- "nlohmann/json is the most popular JSON library for C++ — it lets you parse and build JSON like: json j; j['vid'] = '0951'; std::string body = j.dump();"
- "We check local SQLite FIRST — this is called 'offline-first'. If the server is down, USB monitoring doesn't stop. The local database is always available. The server adds extra information but isn't required for basic operation"
- "The heartbeat thread runs in the background — a separate thread that wakes up every 60 seconds, calls the server, and goes back to sleep. It doesn't block USB monitoring"
- "Event sync uploads the local event queue to the server — every event in the database with synced_to_server=0 gets uploaded. After a successful upload, we set synced_to_server=1 so we don't upload it again"

## You Test
```
1. Start the FastAPI server: uvicorn app.main:app --reload
2. Run the C++ client (as Administrator)
3. Plug in a USB device

Client should now:
a) Print the device info (as before)
b) Check local SQLite allowlist
c) If not found locally: call server POST /api/devices/check
d) Print server decision
e) Upload the event to server POST /api/events

Verify on server: GET http://localhost:8000/api/events
→ Should see the event from your USB plug-in

Test offline: Stop the server, plug in a USB
→ Client should still work using local allowlist
→ Events should queue up (synced=0 in local DB)

Restart server
→ Events should sync (synced=1 in local DB, and appear in GET /api/events)
```

**✅ Task 9 done when: USB events appear on the server AND the client works with server offline.**

---

---

# TASK 10 — Client Registration & Heartbeat (Phase 1J)

**Goal:** The server knows which USBIPS clients exist, their names, and whether they're online.

## Your Prompt to Antigravity

```
I am building USBIPS. I need client registration and heartbeat to work end-to-end.

On the C++ client side:
1. On first startup, generate a persistent client_id: "USBIPS-<HOSTNAME>-<8 random hex chars>"
2. Save it to a config file: usbips_client.conf
   Format: client_id=USBIPS-DESKTOP-A1B2C3D4
3. On every startup:
   a) Read client_id from config
   b) Call ServerClient::RegisterClient(hostname) → POST /api/clients/register
   c) If server responds 200, print "Registered with server"
   d) If server unreachable, print "Server unavailable — running in offline mode"
4. Start heartbeat thread: every 60 seconds call POST /api/clients/heartbeat
5. On shutdown (Ctrl+C): send one final heartbeat with status=OFFLINE

On the server side (update server/app/api/clients.py):
1. POST /api/clients/register: if client_id already exists → UPDATE last_seen, status=ONLINE
                                if new client → INSERT
2. POST /api/clients/heartbeat: update last_heartbeat timestamp, set status=ONLINE
3. Add a background task (APScheduler or FastAPI lifespan): every 5 minutes, mark clients
   as OFFLINE if their last_heartbeat is more than 3 minutes ago

Add: GET /api/clients/status → returns summary:
{ "total": 3, "online": 2, "offline": 1, "clients": [...] }

Explain what a heartbeat is, why we track client status, and what the admin sees.
```

## Antigravity Will Explain While Building
- "A heartbeat is a periodic 'I'm still alive' signal — like a pulse. If the server stops receiving heartbeats from a client, it knows that PC is offline, powered off, or has a network problem"
- "Client registration means the server has a record of every PC running USBIPS — admin can see 'PC-FINANCE-01 is online, PC-RECEPTION-02 is offline since 3pm'"
- "We use APScheduler on the server to run background tasks — like a cron job inside FastAPI. Every 5 minutes it scans all clients and marks stale ones OFFLINE"
- "The config file persists the client_id across reboots — if we generated a new ID every startup, the server would think it's a new machine every time. One persistent ID per machine is the correct design"

## You Test
```
1. Start server
2. Start client
→ Server logs should show: "Client USBIPS-DESKTOP-A1B2C3D4 registered"

3. Call: GET http://localhost:8000/api/clients
→ Should show your PC as ONLINE

4. Stop client (Ctrl+C)
5. Wait 5 minutes
→ Call GET /api/clients again
→ Your PC should now show status: OFFLINE

6. Restart client
→ Should go back to ONLINE
```

**✅ Task 10 done when: client status correctly shows ONLINE/OFFLINE on the server.**

---

---

# TASK 11 — Convert Client to Windows Service (Phase 1K)

**Goal:** USBIPS starts automatically when Windows boots, with no user needing to manually run it.

## Your Prompt to Antigravity

```
I am building USBIPS. The C++ program works correctly. Now I need to convert it to a Windows Service.

A Windows Service runs automatically at startup, even when no user is logged in.

Create: client/src/service/WindowsService.h and WindowsService.cpp

Implement the Windows Service framework:
1. ServiceMain() — entry point called by Windows Service Control Manager
2. ServiceCtrlHandler() — handles START, STOP, PAUSE commands from SCM
3. SetServiceStatus() — reports current status to Windows

The service should:
- On start: initialize everything (logger, database, server client, USB monitor)
- Register for USB device notifications (NOTE: use CM_Register_Notification instead of
  RegisterDeviceNotification — services use a different notification mechanism)
- Start the heartbeat thread
- Run until told to stop
- On stop: clean shutdown of all threads, flush logs, send OFFLINE heartbeat

Add installer code (can be a separate USBIPSInstaller.cpp):
- InstallService(): calls CreateService() to register with Windows SCM
  - Name: "USBIPSClientService"
  - Display name: "USBIPS Client - USB Intrusion Prevention"
  - Start type: SERVICE_AUTO_START (starts at boot)
- UninstallService(): calls DeleteService()

Update main.cpp to:
- If run with argument "--install": call InstallService()
- If run with argument "--uninstall": call UninstallService()
- If run normally: call StartServiceCtrlDispatcher() (service mode)
- If run with "--debug": run as normal console app for debugging

Explain what SCM is, why services use CM_Register_Notification, and how service debugging works.
```

## Antigravity Will Explain While Building
- "SCM stands for Service Control Manager — it's the Windows component that manages all background services, like Windows Update, antivirus, etc. To make something a service, you register it with SCM"
- "Services don't have a user desktop — they run in a separate session. That's why we use CM_Register_Notification instead of RegisterDeviceNotification — the latter requires a window handle (HWND), and services don't have windows"
- "ServiceMain is what Windows calls when your service starts. ServiceCtrlHandler is what Windows calls when you type 'sc stop USBIPSClientService' in the command prompt — your code must respond within a few seconds or Windows kills it"
- "The --debug flag is crucial for development — when you run it as a normal program, you can see console output, attach a debugger, and test quickly. Converting to service hides all console output, so always debug as a console app first"
- "SERVICE_AUTO_START means: start this service when Windows boots, before any user logs in. That's what makes USBIPS a real security tool — it's active before any USB can be plugged in"

## You Test
```
# In command prompt (as Administrator):

# Install the service:
USBIPSClient.exe --install
→ Should print: "USBIPS Client Service installed successfully"

# Start it:
sc start USBIPSClientService
→ Should start

# Check it's running:
sc query USBIPSClientService
→ STATE: RUNNING

# Plug in a USB device
# Check server: GET /api/events → should show the USB event

# Reboot Windows
# After boot, check: sc query USBIPSClientService → RUNNING
# Plug in USB without running anything manually → events appear on server

# Stop and uninstall:
sc stop USBIPSClientService
USBIPSClient.exe --uninstall
```

**✅ Task 11 done when: USBIPS starts automatically after a cold boot with no manual steps.**

---

---

# TASK 12 — Final Phase 1 Integration Test (Phase 1L)

**Goal:** Run the complete 10-test suite from the Phase 1 Testing Plan to confirm everything works end-to-end.

## Your Prompt to Antigravity

```
I am completing Phase 1 of USBIPS. Help me run and document the final integration test.

Create: docs/Phase1_Test_Report.md

Run each test and document the result:

T1  — Known USB storage device:
      Expected: correct identification (VID/PID/Serial) + STORAGE classification + ALLOW
T2  — Known HID keyboard:
      Expected: HID classification + ALLOW
T3  — Unknown USB device:
      Expected: UNKNOWN_DEVICE policy invoked, ASK decision
T4  — Unknown device → user approves:
      Expected: device added to local allowlist + ALLOW + event logged
T5  — Unknown device → user rejects:
      Expected: BLOCK + event logged
T6  — Device revoked from server:
      Expected: PUT /api/devices/{id} with status=REVOKED → client receives updated policy
T7  — USB unplugged:
      Expected: DEVICE_REMOVED event generated + logged
T8  — Server unavailable:
      Expected: client continues using local SQLite policy, logs SERVER_UNAVAILABLE
T9  — Server restored:
      Expected: queued events sync, synced_to_server flips to 1
T10 — Windows reboot:
      Expected: USBIPS Service starts automatically, resumes monitoring

For each test, document:
- Test ID
- Steps performed
- Expected result
- Actual result
- PASS / FAIL
- Screenshot description or log excerpt

Also add to the test report:
- Phase 1 Architecture diagram (ASCII)
- List of all files created
- Phase 1 Completion Checklist against the criteria in the plan
```

## Antigravity Will Explain While Building
- "Integration testing means testing the whole system together — not individual pieces, but the full flow: Windows boot → service start → USB plug → detection → server notification → dashboard update"
- "T6 (remote revocation) is the most important enterprise test — it proves the admin can block a device from the server and the client picks up the change, even without rebooting"
- "T8 + T9 together prove the offline-first design — the system doesn't crash when the server goes away, and it recovers cleanly when it comes back"
- "Writing a test report is not just formality — it's the evidence that your project meets its specification. In a real security product, this becomes the compliance document"

## You Test
Run all 10 tests listed above. Mark each PASS or FAIL. The project is complete when all 10 are PASS.

---

---

# PHASE 1 COMPLETION CHECKLIST

When all 12 tasks are done, verify you can check every box:

```
[ ] USB insertion/removal detected reliably
[ ] VID/PID/Serial/Description extracted for all test devices
[ ] HID, Storage, Network, Other classification works
[ ] Local SQLite allowlist stores trusted devices
[ ] Access controller makes ALLOW / ASK / BLOCK decisions
[ ] Unknown-device approval adds to local allowlist
[ ] Client events recorded to log file and local database
[ ] FastAPI server has all Phase 1 endpoints
[ ] Server stores clients, allowlist, events in SQLite
[ ] C++ client makes REST calls to server
[ ] Events sync from client to server (offline queue + retry)
[ ] Client registers and sends heartbeats
[ ] Server shows client ONLINE/OFFLINE status
[ ] USBIPS runs as Windows Service (automatic startup)
[ ] All T1-T10 integration tests pass
[ ] Phase1_Test_Report.md written
```

**All boxes checked = Phase 1 Complete. Ready for Phase 2 (HID Behavior Detector).**

---

## Files Created At End of Phase 1

### Client (C++ / Visual Studio)
```
client/src/main.cpp
client/src/usb/USBMonitor.h
client/src/usb/USBMonitor.cpp
client/src/device/USBDevice.h
client/src/device/DeviceInfoExtractor.h
client/src/device/DeviceInfoExtractor.cpp
client/src/classifier/DeviceClassifier.h
client/src/classifier/DeviceClassifier.cpp
client/src/access/AccessController.h
client/src/access/AccessController.cpp
client/src/database/LocalDatabase.h
client/src/database/LocalDatabase.cpp
client/src/database/sqlite3.h      ← SQLite amalgamation
client/src/database/sqlite3.c
client/src/logging/EventLogger.h
client/src/logging/EventLogger.cpp
client/src/network/ServerClient.h
client/src/network/ServerClient.cpp
client/src/service/WindowsService.h
client/src/service/WindowsService.cpp
```

### Server (Python / FastAPI)
```
server/requirements.txt
server/app/main.py
server/app/database/database.py
server/app/database/init_db.py
server/app/models/models.py
server/app/models/schemas.py
server/app/api/devices.py
server/app/api/clients.py
server/app/api/events.py
```

### Documentation
```
docs/Phase1_Test_Report.md
README.md
usbips_local.db       ← auto-generated on client
usbips_server.db      ← auto-generated on server
usbips_client.conf    ← auto-generated on client
usbips_events.log     ← auto-generated on client
```

---

## Quick Reference — Task Status Tracker

| # | Task | Phase | Key Deliverable | Status |
|---|------|-------|-----------------|--------|
| 0 | Dev Environment Setup | 0 | Both projects build and run | ⬜ |
| 1 | USB Device Monitor | 1A | Prints on every plug/unplug | ⬜ |
| 2 | Device Info Extractor | 1B | VID/PID/Serial in console | ⬜ |
| 3 | Device Classifier | 1C | HID/Storage/Network assigned | ⬜ |
| 4 | Local SQLite Allowlist | 1D | Allow/block from local DB | ⬜ |
| 5 | Access Controller | 1E | ALLOW/ASK/BLOCK decisions | ⬜ |
| 6 | Event Logger | 1F | Log file + DB events | ⬜ |
| 7 | FastAPI Server | 1G | Server starts, /health works | ⬜ |
| 8 | Server APIs | 1H | All endpoints respond | ⬜ |
| 9 | REST Client + Sync | 1I | Server sees USB events | ⬜ |
| 10 | Heartbeat & Client Mgmt | 1J | ONLINE/OFFLINE status | ⬜ |
| 11 | Windows Service | 1K | Auto-starts on boot | ⬜ |
| 12 | Integration Test | 1L | T1-T10 all PASS | ⬜ |
