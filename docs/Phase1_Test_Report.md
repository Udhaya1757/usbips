# USBIPS Phase 1 Integration Test Report

**Date:** 2026-09-03  
**Scope:** USBIPS Phase 1 Tasks 0-12 only  
**Evidence rule:** Results are marked from executed evidence or explicitly identified prior Task 9-11 evidence. No screenshots were fabricated. Evidence source is terminal output, service state, local logs, and SQLite records.

## Test Environment

| Item | Value |
|---|---|
| Operating system | Microsoft Windows 11 Home Single Language, version 10.0.26200, build 26200 |
| Client | C++ MSVC x64 executable: `client/usbips_client.exe` |
| Service | `USBIPSClient`, display name `USBIPS Client`, `LocalSystem`, `AUTO_START` |
| Service data | `%ProgramData%\\USBIPS\\` |
| Expected service files | `usbips_local.db`, `usbips_events.log`, `usbips_client.conf` |
| Server | Python FastAPI/Uvicorn at `127.0.0.1:8000` |
| Client database | SQLite: `%ProgramData%\\USBIPS\\usbips_local.db` in service mode |
| Server database | SQLite: `server/usbips_server.db` |
| Build toolchain | Visual Studio MSVC x64, vcpkg `x64-windows`, libcurl, nlohmann/json |

The service was found installed and running during this report:

```text
SERVICE_NAME: USBIPSClient
START_TYPE         : 2  AUTO_START
DISPLAY_NAME       : USBIPS Client
SERVICE_START_NAME : LocalSystem
Status              : Running
StartType           : Automatic
```

The service data directory contained the expected configuration, event log, SQLite database, and SQLite WAL/SHM files.

## Architecture

```text
Windows USB Device
       |
       v
USBMonitor
       |
       v
Device Information Extractor
       |
       v
Device Classifier
       |
       v
Access Controller
       |
       +------> Local SQLite Allowlist
       |
       +------> FastAPI Server
       |
       v
ALLOW / ASK / BLOCK
       |
       v
Event Logger
       |
       v
FastAPI Event API
       |
       v
Central Event History
```

```text
Windows Service SCM
       |
       v
USBIPSClient
       |
       v
ApplicationRuntime
       |
       +--> USBMonitor
       +--> ClientManager
       +--> ServerClient
       +--> AccessController
       +--> EventLogger
       +--> LocalDatabase
```

## Test Summary

| Test | Result | Evidence |
|---|---|---|
| T1 | NOT TESTED | No known allowlisted USB storage device was connected during this report. |
| T2 | PARTIAL | Real Dell `413C:301A` HID interface was previously classified as HID; trusted HID ALLOW was not demonstrated. |
| T3 | PARTIAL | Real `413C:301A` interfaces were blocked with `NO_MATCH`; service-only `SERVICE_NO_INTERACTIVE_CONSOLE` behavior was not separately exercised. |
| T4 | NOT TESTED | `AccessController::HandleUserDecision()` supports persistence, but no approval interaction was executed. |
| T5 | NOT TESTED | No separate interactive rejection test was executed. |
| T6 | NOT TESTED | Server has no allowlist mutation/revocation endpoint. |
| T7 | PASS | Prior real USB test logged independent removal events for both `OTHER` and `HID` interfaces. |
| T8 | PASS | Service was RUNNING while port 8000 was not listening; local service files remained available. |
| T9 | PASS | FastAPI was restored and live registration, heartbeat, and device-check requests returned HTTP 200. |
| T10 | PASS | Previously verified during Task 11: service auto-started after reboot. |

## T1 - Known USB Storage Device

| Field | Detail |
|---|---|
| Objective | Verify automatic detection, STORAGE classification, allowlist matching, ALLOW decision, local logging, and server synchronization. |
| Preconditions | A real known allowlisted USB storage device and running USBIPS client/service. |
| Steps performed | No suitable known storage device was connected during this report. |
| Expected result | USB arrival, VID/PID/serial extraction, STORAGE classification, ALLOW, local event, and server event. |
| Actual result | Not tested. The known Kingston sample `0951:1666` was not physically available for this run. |
| Result | **NOT TESTED** |
| Evidence | No fabricated device evidence used. |

## T2 - Known HID Keyboard

| Field | Detail |
|---|---|
| Objective | Verify real HID detection, identity extraction, HID classification, and trusted ALLOW behavior. |
| Preconditions | Real keyboard and matching trusted policy. |
| Steps performed | A previous real-device test observed Dell device `VID=413C`, `PID=301A` and its HID interface. |
| Expected result | HID notification, HID classification, and ALLOW when trusted. |
| Actual result | The HID interface path was independently processed and classified as HID. The device was not demonstrated as trusted; its policy result was `BLOCK / NO_MATCH`. |
| Result | **PARTIAL** |
| Evidence | `HID#VID_413C&PID_301A#7&163b563f&1&0000...` and local log entry `DEVICE_BLOCKED | HID | VID:413C PID:301A`. |

## T3 - Unknown USB Device

| Field | Detail |
|---|---|
| Objective | Verify unknown-device handling in console and service modes. |
| Preconditions | Real device absent from local/server allowlists. |
| Steps performed | Previous real-device testing used Dell `413C:301A` interfaces. The server returned no match. |
| Expected result | Unknown identity is evaluated independently; console may ASK; service must not wait for input and should use its non-interactive policy. |
| Actual result | Both interfaces were independently blocked with server reason `NO_MATCH`. No interactive console ASK or service-specific `SERVICE_NO_INTERACTIVE_CONSOLE` result was captured in this report. |
| Result | **PARTIAL** |
| Evidence | `DEVICE_BLOCKED | OTHER | VID:413C PID:301A ... | BLOCK | NO_MATCH` and `DEVICE_BLOCKED | HID | VID:413C PID:301A ... | BLOCK | NO_MATCH`. |

## T4 - Unknown Device Approval

| Field | Detail |
|---|---|
| Objective | Verify console approval creates a local allowlist record and later produces ALLOW. |
| Preconditions | Unknown real device, console mode, writable local SQLite database. |
| Steps performed | No approval interaction was executed. |
| Expected result | ASK, user approval, `AddDevice()`, USER_APPROVED event, and subsequent ALLOW. |
| Actual result | Not tested. The implementation does contain `AccessController::HandleUserDecision()` and calls `LocalDatabase::AddDevice()` for approval, but this was not demonstrated with a real device. |
| Result | **NOT TESTED** |
| Evidence | Source inspection only; no PASS claimed. |

## T5 - Unknown Device Rejection

| Field | Detail |
|---|---|
| Objective | Verify console rejection results in BLOCK and a USER_REJECTED event without physically disabling USB. |
| Preconditions | Unknown real device and interactive console mode. |
| Steps performed | No separate rejection interaction was executed. |
| Expected result | ASK, user rejection, BLOCK, USER_REJECTED event, and no kernel-level blocking. |
| Actual result | Not tested. |
| Result | **NOT TESTED** |
| Evidence | No fabricated user-input evidence used. |

## T6 - Server Device Revocation

| Field | Detail |
|---|---|
| Objective | Revoke a server allowlist device and verify client policy changes. |
| Preconditions | A server endpoint capable of changing an allowlist policy. |
| Steps performed | Inspected `server/app/api/devices.py`. |
| Expected result | Existing PUT/PATCH/DELETE or equivalent revocation API, followed by a real client policy check. |
| Actual result | The current API provides `GET /api/devices` and `POST /api/devices/check` only. There is no endpoint for adding, updating, revoking, or deleting an allowlist device. No API was invented or added for Task 12. |
| Result | **NOT TESTED** |
| Evidence | `devices.py` contains only the list and check routes; model policy values exist, but no mutation route exists. |

## T7 - USB Unplugged

| Field | Detail |
|---|---|
| Objective | Verify removal detection and independent removal logging. |
| Preconditions | Real USB device connected and USBIPS running. |
| Steps performed | Previous real-device testing connected and removed Dell `413C:301A`, which generated USB and HID interface notifications. |
| Expected result | One removal event per received interface notification, retaining available identity and classification. |
| Actual result | Both removals were processed and logged independently:

```text
DEVICE_REMOVED | OTHER | VID:413C PID:301A SN:6&3b683a3c&0&2
DEVICE_REMOVED | HID   | VID:413C PID:301A SN:7&163b563f&1&0000
```

| Result | **PASS** |
| Evidence | `%ProgramData%\\USBIPS\\usbips_events.log` and prior terminal output. |

## T8 - Server Unavailable

| Field | Detail |
|---|---|
| Objective | Verify the service remains operational when FastAPI is unavailable. |
| Preconditions | Installed service running; FastAPI stopped. |
| Steps performed | During this report, `USBIPSClient` was confirmed `Running` while `Get-NetTCPConnection -LocalPort 8000` reported `Port 8000 is not listening`. Prior Task 11 testing also stopped FastAPI while the service remained running. |
| Expected result | Service and USB monitoring remain alive; local data remains available; server outage does not crash the client. |
| Actual result | Service was still `Running`; service data files existed under `%ProgramData%\\USBIPS\\`. No crash or process disappearance was observed. A local-policy USB decision was not newly generated during this outage window. |
| Result | **PASS** for service resilience; local-policy decision substep not separately tested in this run. |
| Evidence | `Get-Service USBIPSClient`, port check, and ProgramData file listing. |

## T9 - Server Restored

| Field | Detail |
|---|---|
| Objective | Verify communication resumes after FastAPI is restored. |
| Preconditions | USBIPS service running; server restarted at `127.0.0.1:8000`. |
| Steps performed | Started the existing FastAPI application and issued live requests using the persisted service client ID `USBIPS-NAVINESHARAN-fc4c12df`. |
| Expected result | Registration, heartbeat, and policy communication return successfully. |
| Actual result | All live requests returned HTTP 200:

```text
register: 200
heartbeat: 200
 device-check: 200 {"allowed":false,"policy":"BLOCKED","reason":"NO_MATCH","device":null}
```

Uvicorn log:

```text
POST /api/clients/register HTTP/1.1 200 OK
POST /api/clients/heartbeat HTTP/1.1 200 OK
POST /api/devices/check HTTP/1.1 200 OK
```

| Result | **PASS** |
| Evidence | Live PowerShell response and Uvicorn terminal output. |

Queued event synchronization was not claimed. The current client exposes `ServerClient::SendEvent()`, but no complete demonstrated queue-upload workflow was verified in this report.

## T10 - Windows Reboot

| Field | Detail |
|---|---|
| Objective | Verify automatic service startup after Windows reboot. |
| Preconditions | Service installed with `AUTO_START`. |
| Steps performed | Not repeated during Task 12. |
| Expected result | Service starts without manually launching the executable. |
| Actual result | Previously verified during Task 11. The service started automatically after reboot, reached RUNNING, and the `usbips_client.exe` process existed. |
| Result | **PASS** |
| Evidence | Prior Task 11 service status/process evidence. |

## Phase 1 Completion Checklist

| Requirement | Result | Notes |
|---|---|---|
| USB insertion/removal detected reliably | PASS | Real `413C:301A` USB and HID arrival/removal notifications were independently processed. |
| VID/PID/serial extracted for test devices | PASS | `413C`, `301A`, and both interface identifiers were logged. Previous Task 9 evidence also verified `046D:094C`, serial `2508APXHB378`. |
| HID classification works | PASS | Dell HID interface classified as HID. |
| Storage classification works | NOT TESTED | No known real storage device tested in this report. |
| Network/Other classification works where applicable | PASS | Dell USB interface classified as OTHER. |
| Local SQLite allowlist works | PARTIAL | Database exists and access checks execute; trusted ALLOW path was not demonstrated here. |
| Access controller can ALLOW | PARTIAL | Approval code exists; no real trusted ALLOW test was executed here. |
| Access controller can ASK | PARTIAL | Code supports ASK when no server client or server is unavailable; no interactive ASK run was captured here. |
| Access controller can BLOCK | PASS | Real interfaces produced `BLOCK / NO_MATCH`. |
| Unknown-device approval creates allowlist entry | NOT TESTED | No approval test or post-approval SQLite query performed. |
| Events are recorded locally | PASS | ProgramData event log contains connected, blocked, and removal events with identity fields. |
| Events can synchronize with server | PARTIAL | Earlier Task 9 event synchronization was not re-demonstrated; current complete queue synchronization was not claimed. |
| Server centrally manages allowlist entries | PARTIAL | Listing and checking work; revocation/mutation endpoint is missing. |
| Server registers/tracks clients | PASS | Live register and heartbeat returned HTTP 200; server SQLite had three clients and latest client ONLINE. |
| Client operates with local policy when server unavailable | PARTIAL | Service stayed RUNNING during outage; a new local-policy device decision was not generated in this report. |
| USBIPS runs automatically as Windows Service | PASS | Previously verified after reboot; current service is installed, Running, and Automatic. |
| End-to-end demonstration requires no manual client start | PASS | Previously verified through Task 11 reboot evidence. |

## Files Created / Modified Across Tasks 0-12

This report lists the important Phase 1 implementation surfaces, not every repository file.

### Client

| Area | Important files |
|---|---|
| USB monitoring | `client/src/usb/USBMonitor.h`, `client/src/usb/USBMonitor.cpp` |
| Device identification | `client/src/device/USBDevice.h`, `client/src/device/DeviceInfoExtractor.h`, `client/src/device/DeviceInfoExtractor.cpp` |
| Classifier | `client/src/classifier/DeviceClassifier.h`, `client/src/classifier/DeviceClassifier.cpp` |
| Access control | `client/src/access/AccessController.h`, `client/src/access/AccessController.cpp` |
| Database | `client/src/database/LocalDatabase.h`, `client/src/database/LocalDatabase.cpp`, `client/src/database/sqlite3.c`, `client/src/database/sqlite3.h` |
| Logging | `client/src/logging/EventLogger.h`, `client/src/logging/EventLogger.cpp` |
| Network | `client/src/network/ServerClient.h`, `client/src/network/ServerClient.cpp` |
| Service/runtime | `client/src/runtime/ApplicationRuntime.h`, `client/src/runtime/ApplicationRuntime.cpp`, `client/src/service/WindowsService.h`, `client/src/service/WindowsService.cpp`, `client/src/client/ClientManager.h`, `client/src/client/ClientManager.cpp` |
| Console/build | `client/src/console/ConsoleOutput.h`, `client/src/console/ConsoleOutput.cpp`, `client/src/main.cpp`, `client/build.ps1` |

### Server

| Area | Important files |
|---|---|
| FastAPI application | `server/app/main.py`, `server/app/__init__.py` |
| API | `server/app/api/clients.py`, `server/app/api/devices.py`, `server/app/api/events.py`, `server/app/api/schemas.py` |
| Models | `server/app/models/models.py` |
| Database | `server/app/database/database.py`, `server/app/database/init_db.py` |

### Documentation

- `USBIPS_Phase1_Build_Plan.md`
- `phase 1 dcs/USBIPS_Phase_1_Development_Plan.md`
- `DEVELOPMENT_CHECKPOINT.md`
- `docs/Phase1_Test_Report.md`

## Final Status

| Test | Result | Evidence |
|---|---|---|
| T1 | NOT TESTED | No real known USB storage device available for this report. |
| T2 | PARTIAL | Real HID interface classified correctly; trusted ALLOW not demonstrated. |
| T3 | PARTIAL | Real unknown interfaces independently blocked; service-specific ASK conversion not separately demonstrated. |
| T4 | NOT TESTED | Approval persistence not exercised. |
| T5 | NOT TESTED | Interactive rejection not exercised. |
| T6 | NOT TESTED | No server revocation endpoint exists. |
| T7 | PASS | Independent real USB/HID removal events logged. |
| T8 | PASS | Service remained RUNNING with FastAPI unavailable. |
| T9 | PASS | FastAPI restored; live registration, heartbeat, and device-check requests returned 200. |
| T10 | PASS | Previously verified during Task 11. |

**PASS count:** 4  
**FAIL count:** 0  
**NOT TESTED count:** 3  
**PARTIAL count:** 3  

# Phase 1 Overall Status: PARTIAL

The core Phase 1 pipeline and Windows Service operation are supported by real prior evidence: USB interface detection, identity extraction, HID/OTHER classification, application-level BLOCK decisions, local event logging, service auto-start, service resilience, registration, heartbeat, and server policy checks.

Remaining Phase 1 gaps are:

1. No known real USB storage ALLOW test was available for this report.
2. Approval and rejection interactions were not executed in this test run.
3. The server lacks an allowlist mutation/revocation endpoint, so central revocation cannot be tested.
4. Complete queued event synchronization was not demonstrated and is not claimed as PASS.
5. Local-policy decision behavior during a live server outage was not newly exercised in this report.

No out-of-scope security features were added. No kernel driver, Minifilter, behavioral analytics, ML, deduplication, manual VID/PID workflow, or future-phase implementation was performed.
server/app/api/clients.py
server/app/api/devices.py
server/app/api/events.py
server/app/api/schemas.py
server/app/database/database.py
server/app/database/init_db.py
server/app/models/models.py