> From: https://chatgpt.com/share/6a941a25-2530-83e8-b855-1a38f9c41cf1?ogimg=plain

# you asked

message time: 2026-08-26 21:17:55

This is the base paper for a project I am going to build, completely go through the paper and give 
what are the tools and technologies I have to use? 
What is the detailed design flow?
What parts are feasible?

---

# chatgpt response

Yes. I went through the paper as the primary source, including its architecture, workflow, implementation approach, experiments, benchmarks, limitations, and future-work section. The paper is **“USBIPS framework: Protecting hosts from malicious USB peripherals”**, published in *Computer Standards & Interfaces* (2026). 

The important thing is: **you do not need to reproduce every part of the paper exactly**. Some parts are very feasible for a student project, while others involve Windows kernel/driver development and are considerably harder.

---

# 1. What exactly are you building?

The paper's USBIPS is essentially a **Windows-based USB Intrusion Prevention System** with two major components:

```text
                    ┌─────────────────────────┐
                    │      USBIPS SERVER      │
                    │                         │
                    │ • Client Monitor        │
                    │ • Log Analyzer           │
                    │ • Allowlist Manager      │
                    │ • Behavior Rule Manager  │
                    └───────────┬─────────────┘
                                │ REST + JSON
                         Heartbeats / Rules
                                │
              ┌─────────────────┴─────────────────┐
              │                                   │
      ┌───────▼────────┐                  ┌───────▼────────┐
      │ USBIPS CLIENT  │                  │ USBIPS CLIENT  │
      │                │                  │                │
      │ Windows PC     │                  │ Windows PC     │
      │                │                  │                │
      │ Device Monitor │                  │ Device Monitor │
      │ Classifier     │                  │ Classifier     │
      │ Allowlist      │                  │ Allowlist      │
      │ HID Detector   │                  │ HID Detector   │
      │ Storage        │                  │ Storage        │
      │ Network        │                  │ Network        │
      └───────┬────────┘                  └────────────────┘
              │
        Windows API
              │
      ┌───────▼────────┐
      │ Windows Kernel │
      │ USB subsystem  │
      └───────┬────────┘
              │
          USB Device
```

The paper explicitly separates the system into a **USBIPS client and USBIPS server**. The client handles USB devices and detection, while the server provides centralized policy and log management. 

---

# 2. Technologies you need

## A. Core platform

### Operating System

**Windows 10/11**

Prefer:

- Windows 11
- x64
- Administrator privileges
- Ideally a separate test machine/VM

The paper itself evaluated the clients on Windows 11 24H2. 

---

## B. USB client

This is the most important part.

### Programming language

**C/C++**

The paper states that most of its client functionality was implemented in C/C++ using the Windows API. 

I strongly recommend:

> **C++ for the USBIPS client**

because you will need Windows APIs, device structures, Windows services, raw input, filesystem monitoring, etc.

---

## C. Windows APIs

You will need several Windows APIs.

### 1. Device detection

The paper uses:

- `RegisterDeviceNotification()`
- `WM_DEVICECHANGE`
- USB device interface GUIDs

The paper registers notifications for:

- HID
- Storage
- Network
- Other USB devices



Microsoft currently documents `RegisterDeviceNotification()` for receiving device notifications, and also provides `CM_Register_Notification()` for newer Windows versions. ([Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerdevicenotificationa?utm_source=chatgpt.com))

For your implementation:

```text
USB plugged in
      ↓
Windows PnP notification
      ↓
USBIPS daemon
      ↓
Identify device
```

---

### 2. USB device information

You need to extract things such as:

- Device type
- USB bus
- VID
- PID
- Product revision
- Serial number
- Device key
- Drive letter
- Volume serial
- Filesystem
- Device description

The paper's `CDevice` structure explicitly contains these fields. 

This is where the **VID/PID/Serial Number** information you previously asked me about becomes important.

---

# 3. Device classification

Your classifier should initially support exactly the three categories from the paper:

```text
USB device
    │
    ├── HID
    │     └── Keyboard
    │
    ├── Storage
    │     └── Flash drive / HDD
    │
    └── Network
          └── USB Wi-Fi/Ethernet
```

The paper specifically targets HIDs, storage devices and network adapters. 

You can later add:

- Mouse
- Webcam
- Audio
- Bluetooth
- Serial/COM
- Game controller

But **don't start with those**.

---

# 4. Allowlisting system

This is one of the easiest and most important modules.

Example database:

```text
Device ID
VID
PID
Serial Number
Device Type
Volume Serial
Device Description
Status
Date Added
```

Example:

```text
VID      PID      Serial       Type
0951     1666     ABC123       STORAGE
046D     C52B     XYZ456       HID
0BDA     8176     DEF789       NETWORK
```

The paper's allowlist uses detailed device information such as VID, serial number and partition volume information. 

### Decision logic

```text
Device detected
      ↓
Extract identifiers
      ↓
Search allowlist
      ↓
 ┌────┴────┐
MATCH     NO MATCH
  ↓          ↓
ALLOW     Ask user
             ↓
        ┌────┴────┐
       YES        NO
        ↓          ↓
     Add to      Block
    allowlist
```

This part is **highly feasible**.

---

# 5. Database

The original paper uses:

> Microsoft SQL Server Compact + OLE DB



However, **I would NOT recommend copying this choice today**.

SQL Server Compact 4.0 reached extended support end on **July 13, 2021**. ([Microsoft Learn](https://learn.microsoft.com/en-us/lifecycle/products/microsoft-sql-server-compact-40?utm_source=chatgpt.com))

Use:

### Recommended

**SQLite**

For example:

```text
usbips.db

Devices
--------
id
vid
pid
serial
device_type
description
allowed
created_at

Events
--------
id
device_id
event_type
severity
process
path
timestamp

Rules
--------
id
device_type
rule_type
rule_value
action
```

SQLite is much more sensible for a student implementation.

---

# 6. HID behavior detector

This is the most interesting part of the project.

The paper uses a **CAPTCHA-based active defense**.

When a new keyboard/HID appears:

```text
New HID detected
       ↓
Block keyboard input
       ↓
Generate random CAPTCHA
       ↓
Ask user to type CAPTCHA
       ↓
       ├── Correct
       │     ↓
       │  Allow keyboard
       │     ↓
       │  Continue behavior monitoring
       │
       └── Wrong
             ↓
       Keep keyboard blocked
             ↓
           Alert
```

The paper generates the CAPTCHA using Windows `BCryptGenRandom()` and eight random bytes. 

The paper demonstrated this against a Rubber Ducky-style masquerade keyboard: the malicious device attempted to type commands, but USBIPS blocked the keystrokes. 

### Technologies

- C++
- Windows Raw Input
- Keyboard hooks
- `BCryptGenRandom`
- Windows GUI
- HID APIs

---

# 7. Storage behavior detector

This is another **very feasible** component.

The paper's approach:

```text
USB storage connected
        ↓
Allowlist verification
        ↓
Allow device
        ↓
Monitor file activity
        ↓
Is sensitive file/path being accessed?
       / \
     YES  NO
      ↓    ↓
    ALERT Continue
      ↓
   Block/undo
```

The paper monitors:

- Full file path
- Process ID
- Process name
- Read time
- Write time



The paper used **FileActivityWatch** for this experiment. 

### For your project

You can initially implement:

```text
Protected directories:
C:\Users\<user>\Documents\Confidential
C:\Users\<user>\Desktop\Sensitive
```

Then detect:

```text
USB → read sensitive file → USB
```

or

```text
Sensitive folder → USB drive
```

and generate:

```text
⚠ DATA EXFILTRATION DETECTED

Device: USB Storage
Source: C:\Confidential\passwords.txt
Destination: E:\
Process: explorer.exe
Action: BLOCKED
```

This is an excellent demonstration component.

---

# 8. Network adapter detector

The paper monitors USB network adapters for:

- DHCP changes
- DNS server changes
- Gateway changes
- DNS query anomalies



The paper used **DNSQuerySniffer** for its experiment. 

Architecture:

```text
USB Wi-Fi adapter
       ↓
Detect new network interface
       ↓
Take baseline configuration
       ↓
Monitor:
   DHCP
   Gateway
   DNS
       ↓
DNS query
       ↓
Compare expected result
       ↓
 ┌─────┴─────┐
Normal     Abnormal
  ↓            ↓
Allow        Alert
             ↓
      Restore DNS settings
```

This is feasible, but **harder than the storage detector**.

---

# 9. Central server

The server is a web application.

The paper explicitly uses:

> REST API + JSON



It contains four major components:

### 1. Client Status Monitor

Shows:

```text
Client 001 → Online
Client 002 → Online
Client 003 → Offline
```

### 2. Log Analyzer

Displays:

```text
21:10 USB connected
21:10 HID detected
21:10 Allowlist matched
21:10 CAPTCHA failed
21:10 DEVICE BLOCKED
```

### 3. Allowlist Manager

Administrator can:

```text
Add device
Remove device
Approve device
Block device
```

### 4. Behavior Rule Manager

Administrator can define:

```text
Storage:
Protected path = C:\Confidential

Network:
Allowed DNS = 8.8.8.8

HID:
CAPTCHA required = true
```

The paper explicitly describes these four server services. 

---

# 10. What should you use for the server?

The paper only specifies the architecture, not that you must use a particular web framework.

I recommend:

### Option A — easiest

**Python + FastAPI**

```text
FastAPI
   ↓
REST API
   ↓
SQLite/PostgreSQL
   ↓
Web Dashboard
```

Frontend:

- React
- HTML/CSS/JavaScript

For a college project, you can even use:

> **FastAPI + simple HTML/JS dashboard**

rather than React.

---

# 11. Complete technology stack I recommend

| Component | Technology |
|---|---|
| OS | Windows 11 |
| Client | C++ |
| USB detection | Windows API / PnP |
| Device notification | RegisterDeviceNotification / CM_Register_Notification |
| USB identification | Windows Device APIs |
| Device classification | C++ |
| Allowlist | SQLite |
| HID monitoring | Raw Input / keyboard hooks |
| CAPTCHA | C++ + BCryptGenRandom |
| File monitoring | Windows APIs / FileActivityWatch for prototype |
| Network monitoring | Windows networking APIs |
| DNS monitoring | DNSQuerySniffer for prototype |
| Client service | Windows Service |
| Communication | REST |
| Data format | JSON |
| Server | Python FastAPI |
| Dashboard | HTML/CSS/JS or React |
| Server DB | SQLite/PostgreSQL |
| Logging | JSON + database |
| Testing | Windows VM + physical USB devices |
| Driver development | Visual Studio + WDK |
| Advanced protection | KMDF / Minifilter |

For current Windows driver development, Microsoft recommends Visual Studio with the Windows Driver Kit (WDK); the current documentation supports Visual Studio 2026/WDK for current Windows 11 driver development. ([Microsoft Learn](https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/?utm_source=chatgpt.com))

---

# 12. Detailed design flow

This is the flow I would actually implement.

```text
                  ┌──────────────────┐
                  │ Windows Startup  │
                  └────────┬─────────┘
                           ↓
                  ┌──────────────────┐
                  │ USBIPS Service   │
                  └────────┬─────────┘
                           ↓
                  ┌──────────────────┐
                  │ Start USB Daemon │
                  └────────┬─────────┘
                           ↓
                  Monitor USB events
                           │
                  USB DEVICE CONNECTED
                           ↓
              ┌─────────────────────────┐
              │ Device Identification  │
              │ VID / PID / Serial     │
              │ Class / Description    │
              └────────────┬────────────┘
                           ↓
              ┌────────────────────────┐
              │ Device Classification  │
              └────────────┬────────────┘
                           ↓
             ┌─────────────┼─────────────┐
             ↓             ↓             ↓
           HID          STORAGE        NETWORK
             │             │             │
             └─────────────┼─────────────┘
                           ↓
                  ┌─────────────────┐
                  │ Allowlist Check │
                  └────────┬────────┘
                           ↓
                  ┌────────┴────────┐
                  │                 │
                MATCH            NO MATCH
                  │                 │
                  ↓                 ↓
                ALLOW          User decision
                                    │
                              ┌─────┴─────┐
                              ↓           ↓
                            ALLOW       BLOCK
                              │
                              ↓
                  Behavior Monitoring
                              │
              ┌───────────────┼───────────────┐
              ↓               ↓               ↓
          HID Rules       Storage Rules    Network Rules
              │               │               │
              ↓               ↓               ↓
          CAPTCHA       Sensitive path     DNS/DHCP
          / input        access             changes
              │               │               │
              └───────────────┼───────────────┘
                              ↓
                      ┌───────────────┐
                      │ Anomaly found?│
                      └───────┬───────┘
                              ↓
                        ┌─────┴─────┐
                        │           │
                       NO          YES
                        │           │
                        ↓           ↓
                     Continue    BLOCK
                                  ↓
                               ALERT
                                  ↓
                              Log event
                                  ↓
                       ┌───────────────┐
                       │ USBIPS Server │
                       └───────────────┘
                                  ↓
                         Log Analyzer
                                  ↓
                         Administrator
```

This follows the paper's actual workflow: service activation → device connection → classification → allowlisting → behavior detection → response → logging.  

---

# 13. How the three detectors work together

This is the **core novelty of your project**.

## Scenario 1 — Normal keyboard

```text
Keyboard
 ↓
Recognized
 ↓
Allowlisted
 ↓
CAPTCHA
 ↓
Correct
 ↓
Normal operation
```

---

## Scenario 2 — Rubber Ducky / masquerade keyboard

```text
Malicious USB
 ↓
Pretends to be keyboard
 ↓
VID/PID may look legitimate
 ↓
Allowlist can be bypassed
 ↓
HID behavior detector
 ↓
CAPTCHA
 ↓
Automated payload cannot correctly interact
 ↓
Keystrokes blocked
 ↓
ALERT
```

This demonstrates why **allowlisting alone is insufficient**. The paper deliberately used a masquerade device that duplicated identifiers of legitimate keyboards and still relied on behavior detection to catch it. 

---

## Scenario 3 — Data theft

```text
Masquerade USB storage
 ↓
Passes allowlist
 ↓
Mounted
 ↓
Attempts:
C:\Confidential\file.txt
              ↓
             USB
 ↓
File monitor
 ↓
Sensitive path detected
 ↓
BLOCK + ALERT + LOG
```

This is particularly good for your demonstration.

---

## Scenario 4 — USB Wi-Fi attack

```text
USB Wi-Fi adapter
 ↓
Classification
 ↓
Allowlist
 ↓
Network monitoring
 ↓
DNS/DHCP changed
 ↓
Compare DNS response
 ↓
Mismatch
 ↓
Restore configuration
 ↓
ALERT
```

This is based directly on the paper's network detector. 

---

# 14. What is actually feasible?

This is where I would **not** blindly copy the paper.

## 🟢 Very feasible

### 1. USB device detection

**Difficulty: 2/10**

Detect:

- Plug
- Unplug
- VID
- PID
- Serial
- Device class
- Description

Absolutely feasible.

---

### 2. Device classification

**Difficulty: 3/10**

```text
HID
Storage
Network
Other
```

Very feasible.

---

### 3. Allowlisting

**Difficulty: 3/10**

Probably the easiest security feature to demonstrate.

---

### 4. Centralized logging

**Difficulty: 3/10**

Very feasible.

---

### 5. REST server

**Difficulty: 3/10**

FastAPI + SQLite makes this straightforward.

---

### 6. Dashboard

**Difficulty: 3/10**

You can show:

```text
Connected devices
Allowed devices
Blocked devices
Alerts
Client status
Device history
```

Very feasible.

---

### 7. Storage exfiltration detection

**Difficulty: 5/10**

Definitely feasible as a project feature.

---

### 8. HID/Rubber-Ducky detection

**Difficulty: 6/10**

Feasible, but input interception is considerably more difficult than simple device enumeration.

Still, this is probably the **best attack demonstration**.

---

# 15. Moderately difficult

## 🟡 Network adapter detection

**Difficulty: 6–7/10**

Detecting:

```text
New USB network adapter
       ↓
DHCP change
DNS change
Gateway change
```

is feasible.

But doing deep USB packet-level inspection is much harder.

The paper itself acknowledges that tracking USB network packet sources/destinations through modern OS networking layers is difficult. 

Therefore:

> Don't try to inspect raw USB network packets for your first implementation.

Monitor the **network configuration and DNS behavior** instead.

---

# 16. Difficult part: Kernel module

## 🔴 Windows kernel/driver development

This is the biggest challenge.

The paper's architecture includes:

```text
User space
   ↓
Windows API
   ↓
Kernel
   ↓
HCI
   ↓
USB
```



The paper itself acknowledges that its user-space design only detects/controls devices after the driver is loaded. 

### Do you need to implement this?

**For your first version: NO.**

You can build the project primarily in user space.

---

# 17. Very difficult: Windows Minifilter

The paper itself proposes moving the client into the driver layer using a **Windows Minifilter** as future work. 

A minifilter can operate at the filesystem level and intercept file I/O before it reaches its target. Microsoft describes minifilters as kernel-mode components capable of logging, observing, modifying, or preventing file I/O. ([Microsoft Learn](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/about-file-system-filter-drivers?utm_source=chatgpt.com))

This would make your project much more advanced:

```text
USB
 ↓
Windows Kernel
 ↓
Minifilter
 ↓
Detect/block
 ↓
Filesystem
```

But:

**Do not make this mandatory for the first implementation.**

You can list it as:

> **Future enhancement / advanced version**

---

# 18. Very difficult: Service self-protection

The paper has an unusual mechanism where its service observer and daemon protect each other, with the observer injected into `services.exe`. 

I **would not recommend reproducing this exactly** in a student project.

It adds significant complexity and creates a very invasive implementation.

Instead:

```text
Windows Service
       ↓
Watchdog
       ↓
Check USBIPS process
       ↓
Restart if stopped
```

This gives you a clean demonstration without making process injection a core dependency.

---

# 19. The most realistic project scope

If this is a **college major project**, I would build this:

## Phase 1 — Core USBIPS

```text
Windows client
       │
       ├── USB detection
       ├── Device classification
       ├── VID/PID/Serial extraction
       ├── Allowlisting
       ├── Device blocking
       └── Logging
```

---

## Phase 2 — Behavior detection

```text
       ├── HID behavior detector
       │      └── CAPTCHA
       │
       ├── Storage behavior detector
       │      └── Sensitive-file monitoring
       │
       └── Network behavior detector
              └── DNS/DHCP monitoring
```

---

## Phase 3 — Central server

```text
USBIPS Server
   │
   ├── Client management
   ├── Allowlist management
   ├── Behavior rules
   ├── Log collection
   ├── Alert dashboard
   └── Device history
```

---

# 20. What I would NOT implement initially

Don't try to reproduce all of these:

| Feature | Recommendation |
|---|---|
| Raw USB packet inspection | ❌ Skip |
| Custom USB kernel driver | ❌ Skip initially |
| Windows Minifilter | 🟡 Advanced |
| Full firmware analysis | ❌ Skip |
| Physical-layer attacks | ❌ Skip |
| USB electrical attacks | ❌ Skip |
| DMA attacks | ❌ Out of scope |
| Process injection into `services.exe` | ❌ Replace with watchdog |
| ML detection | 🟡 Optional future enhancement |
| Hundreds of USB device classes | ❌ Skip |
| Enterprise-scale thousands of clients | ❌ Demonstrate with 1–3 |

The paper itself excludes physical-damage attacks and assumes the host OS/hardware/drivers are not malicious. 

---

# 21. Best attack demonstrations

For your final demo, I would use **three attacks**, exactly aligned with the paper:

### Demo 1 — Masquerade keyboard

Use a controlled USB HID test device.

```text
USB HID
 ↓
Device classifier
 ↓
CAPTCHA
 ↓
Automated keystrokes blocked
 ↓
Alert
```

### Demo 2 — USB data theft

```text
USB storage
 ↓
Access sensitive file
 ↓
Detector
 ↓
BLOCK
 ↓
Alert + log
```

### Demo 3 — DNS manipulation

```text
USB network adapter
 ↓
DNS configuration change
 ↓
Detector
 ↓
DNS result mismatch
 ↓
Restore configuration
 ↓
Alert
```

These correspond directly to the paper's evaluated BadUSB, masquerade-storage/data-theft, and DNS-spoofing cases. 

---

# 22. One important improvement I recommend

Your project can actually be **better than simply reproducing the paper**.

The paper's own limitations say:

- detection happens after the driver is loaded;
- storage/network rules aren't standardized;
- HID detection depends on user interaction;
- the HID method can disturb legitimate users;
- it mainly handles keyboards rather than all HIDs;
- new IoCs may require updating the client software. 

So your project could introduce:

### **Rule-based USB behavior engine**

Instead of hard-coding:

```cpp
if (path == "C:\\Confidential")
    block();
```

create:

```json
{
    "rule_id": "USB-STORAGE-001",
    "device_type": "storage",
    "event": "FILE_COPY",
    "source": "C:\\Confidential\\*",
    "destination": "USB:*",
    "action": "BLOCK",
    "severity": "HIGH"
}
```

Then your server can send new rules to clients.

That directly addresses one of the paper's limitations and gives your project an actual **extension over the base paper**, rather than being a copy.

The authors themselves suggest standardized rules such as YARA and eventually ML-based behavioral detection as future work. 

---

# 23. Recommended final architecture

I'd therefore build **this version**:

```text
                         USBIPS SERVER
                 ┌──────────────────────────┐
                 │ FastAPI REST API         │
                 │                          │
                 │ Client Manager            │
                 │ Allowlist Manager         │
                 │ Rule Manager              │
                 │ Log Analyzer              │
                 │ Alert Manager             │
                 └────────────┬─────────────┘
                              │
                         REST / JSON
                              │
               ┌──────────────┴──────────────┐
               │                             │
       ┌───────▼─────────┐           ┌──────▼─────────┐
       │ USBIPS CLIENT   │           │ USBIPS CLIENT  │
       │                 │           │                │
       │ Windows Service │           │ Windows Service│
       │                 │           │                │
       │ USB Monitor     │           │ USB Monitor    │
       │ Device Parser   │           │ Device Parser  │
       │ Classifier      │           │ Classifier     │
       │ Allowlist       │           │ Allowlist      │
       │ Rule Engine     │           │ Rule Engine    │
       │                 │           │                │
       │ ┌─────────────┐ │           │                │
       │ │HID Detector │ │           │                │
       │ │Storage      │ │           │                │
       │ │Network      │ │           │                │
       │ └─────────────┘ │           │                │
       │                 │           │                │
       │ SQLite          │           │ SQLite         │
       └────────┬────────┘           └────────────────┘
                │
          Windows APIs
                │
       ┌────────▼────────┐
       │ Windows USB/PnP │
       │    subsystem    │
       └────────┬────────┘
                │
              USB
                │
      ┌─────────┼──────────┐
      ↓         ↓          ↓
    HID      Storage     Network
```

---

## My feasibility assessment

| Module | Difficulty | Recommendation |
|---|---:|---|
| USB detection | ⭐⭐ | **Definitely build** |
| VID/PID/Serial extraction | ⭐⭐ | **Definitely build** |
| Device classification | ⭐⭐ | **Definitely build** |
| Allowlisting | ⭐⭐ | **Definitely build** |
| SQLite database | ⭐ | **Definitely build** |
| Windows service | ⭐⭐⭐ | **Build** |
| Central REST server | ⭐⭐⭐ | **Build** |
| Dashboard | ⭐⭐⭐ | **Build** |
| Central logging | ⭐⭐ | **Build** |
| Storage behavior detector | ⭐⭐⭐⭐ | **Build** |
| HID CAPTCHA defense | ⭐⭐⭐⭐⭐ | **Build** |
| DNS/DHCP detector | ⭐⭐⭐⭐⭐ | **Build if time permits** |
| Network packet inspection | ⭐⭐⭐⭐⭐⭐⭐ | **Don't initially** |
| Kernel USB driver | ⭐⭐⭐⭐⭐⭐⭐⭐ | **Optional advanced** |
| Windows Minifilter | ⭐⭐⭐⭐⭐⭐⭐⭐ | **Future enhancement** |
| Firmware analysis | ⭐⭐⭐⭐⭐⭐⭐⭐⭐ | **Don't build** |
| ML behavior detector | ⭐⭐⭐⭐⭐⭐⭐ | **Future enhancement** |

### Bottom line

**Yes, the project is feasible.** But a full one-to-one reproduction of the paper is **too ambitious** if you are doing it as a student project.

The sweet spot is:

> **Windows C++ USB client + device identification/classification + allowlisting + HID behavior detection + storage exfiltration detection + optional DNS/DHCP detection + FastAPI centralized server + dashboard + logging.**

That gives you a **real, demonstrable USB IPS**, while avoiding the hardest kernel-level components.

Also, I would explicitly mention in your project report that **SQL Server Compact is replaced with SQLite** because the paper's database choice is now out of support. ([Microsoft Learn](https://learn.microsoft.com/en-us/lifecycle/products/microsoft-sql-server-compact-40?utm_source=chatgpt.com))

And importantly, the paper itself achieved **98.21% of baseline file I/O throughput**, with 8.49% CPU overhead and 60.8 MB memory overhead, so you also have concrete performance metrics to reproduce and compare against. 

If you build it this way, **the next logical step is to divide the project into modules and implement them one by one**, starting with the Windows USB device detector and VID/PID/Serial extraction, rather than starting with the server.

---

# you asked

message time: 2026-08-26 21:52:05

Is all the types of attacks are feasible, can I able to replicate the attack?

---

# chatgpt response

Yes — **the attacks that the paper actually demonstrates are feasible to reproduce in a controlled lab**, but they are not all equally easy. More importantly, you **do not need the exact commercial attack hardware used in the paper** to demonstrate the security mechanism.

The paper's evaluation actually tested three main attack scenarios: **BadUSB/masquerade keyboard, masquerade storage/data theft, and DNS spoofing through a USB network adapter**. 

## Feasibility of each attack

| Attack in paper | Can you reproduce? | Difficulty | Recommended? |
|---|---|---:|---|
| **BadUSB / Rubber Ducky keyboard** | ✅ Yes | Medium | ⭐⭐⭐⭐⭐ |
| **Masquerade storage / Hermes** | ✅ Yes, with suitable USB device | Medium–Hard | ⭐⭐⭐⭐⭐ |
| **Data theft through USB storage** | ✅ Yes | Easy–Medium | ⭐⭐⭐⭐⭐ |
| **DNS spoofing via USB Wi-Fi adapter** | ✅ Yes in isolated lab | Medium | ⭐⭐⭐⭐ |
| Normal USB device bypassing allowlist | ✅ Yes | Easy | ⭐⭐⭐⭐⭐ |
| Other attacks merely discussed in paper | ⚠️ Not necessarily | Varies | Not required |

---

# 1. BadUSB / Rubber Ducky attack — YES

This is probably the **best attack to demonstrate**.

The paper used a USB Rubber Ducky configured to behave as a keyboard. When connected, it automatically sent a sequence of keystrokes. Without protection, the paper demonstrated that it could open applications and execute commands; with USBIPS, the keystrokes were blocked because the device had not passed the CAPTCHA interaction. 

Your demonstration can be:

```text
              USB HID
                 ↓
          Windows detects
                 ↓
          USBIPS detects HID
                 ↓
          CAPTCHA appears
                 ↓
      ┌──────────┴──────────┐
      │                     │
  Legit keyboard       Automated HID
      │                     │
 CAPTCHA succeeds      Cannot respond
      │                     │
    ALLOW                BLOCK
```

### What you need

You can use:

- a Rubber Ducky, **or**
- another programmable USB HID device, **or**
- a development board capable of USB HID emulation.

You don't necessarily need to reproduce the exact payload from the paper. The important property is:

> **The USB device automatically generates keyboard input immediately after connection.**

That's what your detector needs to identify.

### What you should demonstrate

For example, have the test HID attempt to perform a harmless action such as opening Notepad.

Your system should show:

```text
[USB CONNECTED]

Type: HID Keyboard
VID: xxxx
PID: xxxx

Behavior verification required.

CAPTCHA: XXXXXXXX

Automated keyboard input detected/blocked.

THREAT: HID MASQUERADE
ACTION: BLOCK
```

The paper itself specifically reports that the Rubber Ducky was initially able to pass the allowlist because it duplicated identifiers of legitimate keyboards, demonstrating why allowlisting alone is insufficient. 

**This is an excellent final-demo attack.**

---

# 2. Hermes / masquerade storage attack — YES, but harder

This one is slightly different.

The paper used **Hermes to simulate a Transcend JetFlash 16 GB flash drive**. The simulated device passed the allowlist because it presented identifiers matching a legitimate storage device. It was then mounted as the F: drive. 

The attack was essentially:

```text
             Malicious USB
                  ↓
       Pretends to be legitimate
             flash drive
                  ↓
          VID/PID/Serial
             match
                  ↓
           ALLOWLIST PASSED
                  ↓
              Mounted
                  ↓
       Sensitive files accessed
                  ↓
              USB drive
                  ↓
       DATA EXFILTRATION
```

### The important point

You **don't necessarily need Hermes itself**.

For your project, you can reproduce the **security scenario**:

> A USB storage device that is trusted/allowlisted attempts to copy protected information to removable storage.

That's actually more useful for demonstrating your detector.

---

# 3. USB data-theft attack — VERY feasible

This is the easiest attack from the paper to reproduce.

The paper created a `confidential` directory and copied files from the host's C: drive to the USB storage device. USBIPS detected the abnormal file activity and generated an alarm containing the file paths, processes and read/write information. 

You can create:

```text
C:\USBIPS_Test\Confidential\
    employee_data.txt
    project_design.pdf
    secret.txt
```

and use:

```text
E:\
```

as the USB drive.

Then:

```text
C:\USBIPS_Test\Confidential\secret.txt
                    ↓
                  READ
                    ↓
                 USB E:\
                    ↓
                 WRITE
                    ↓
             USBIPS detects
                    ↓
                 ALERT
```

Your UI can show:

```text
🚨 USB DATA EXFILTRATION

Source:
C:\USBIPS_Test\Confidential\secret.txt

Destination:
E:\secret.txt

Device:
Kingston USB

Process:
explorer.exe

Severity:
HIGH

Action:
BLOCKED
```

This is **100% feasible** as a controlled demonstration.

---

# 4. DNS spoofing through USB network adapter — YES

This one is also reproducible, but requires a proper isolated test network.

The paper connected a USB Wi-Fi adapter and simulated a DNS spoofing scenario using a DNS server in the same test network. It then detected abnormal DNS results and demonstrated redirection from a legitimate domain to a malicious one. 

The architecture is:

```text
USB Wi-Fi adapter
       ↓
Windows detects new NIC
       ↓
USBIPS identifies NETWORK device
       ↓
Allowlist
       ↓
Monitor DNS/DHCP configuration
       ↓
DNS response changes
       ↓
USBIPS detects anomaly
       ↓
ALERT
```

### Important

You should do this entirely inside a **closed lab network**, preferably:

```text
             ┌──────────────┐
             │ Test Router  │
             └──────┬───────┘
                    │
          ┌─────────┴─────────┐
          │                   │
    Test Windows PC      Test DNS server
     + USB Wi-Fi             VM
```

You don't need to attack a real website or public DNS infrastructure.

You can use a **fake test domain** and demonstrate:

```text
test.example
     ↓
Expected → 192.168.x.x
Actual   → 192.168.x.y
     ↓
DNS anomaly detected
```

That is actually cleaner and safer for your project.

---

# 5. What about the other attacks mentioned in the paper?

This is where we need to distinguish **“discussed” from “experimentally reproduced.”**

The paper discusses many USB attack classes, including:

- BadUSB
- Rubber Ducky
- Hermes
- masquerade keyboard
- masquerade mouse
- masquerade webcam
- data theft
- DNS-related attacks
- other USB peripheral attacks

But the paper's actual evaluation focuses on:

### HID

**Rubber Ducky / masquerade keyboard**

### Storage

**Hermes / masquerade flash drive + data theft**

### Network

**USB Wi-Fi adapter + DNS spoofing**

The authors themselves state that their detector mainly focuses on **HIDs, storage devices, and network adapters**. 

So **don't tell your project evaluator that you are implementing every USB attack mentioned in the paper.**

Say:

> "The system focuses on three representative USB attack categories: HID masquerading/BadUSB, storage-based data exfiltration, and network-adapter-based DNS manipulation."

That's technically much more accurate.

---

# 6. What about masquerade mouse/webcam?

I would **not implement these**.

The paper itself says its HID detector primarily focuses on keyboards and acknowledges that masquerade mice and webcams remain problems. For example, it discusses a malicious mouse performing automated clicks and a masquerade webcam recording data. 

So:

```text
Keyboard → ✅ Implement
Mouse    → ❌ Not necessary
Webcam   → ❌ Not necessary
Gamepad  → ❌ Not necessary
```

In fact, this limitation can become part of your **future work**.

---

# 7. One particularly important attack scenario

You should definitely demonstrate this:

### **Allowlist bypass**

Because this is one of the strongest points of the paper.

Suppose:

```text
Legitimate keyboard

VID = 1234
PID = 5678
Serial = ABC123

             ↓

Allowlist
             ↓
           ALLOW
```

Now your malicious HID presents the same identifiers:

```text
Malicious HID

VID = 1234
PID = 5678
Serial = ABC123

             ↓

Allowlist
             ↓
           ALLOW
```

Therefore:

> **VID/PID/Serial-based allowlisting alone cannot establish that the physical device is trustworthy.**

The paper demonstrated exactly this problem: the Rubber Ducky duplicated identifiers of real keyboards and therefore wasn't blocked by allowlisting. 

Then your behavior detector catches it:

```text
Allowlist:
       PASS ✅

        ↓

Behavior:
       Suspicious ❌

        ↓

CAPTCHA:
       Failed / automated

        ↓

BLOCK 🚫
```

**This is probably the strongest demonstration of why your project needs both allowlisting + behavior detection.**

---

# 8. So can you replicate the paper's evaluation?

### Yes.

I'd structure your final testing like this:

| Test | Attack/device | Expected result |
|---|---|---|
| T1 | Normal USB flash drive | Allow |
| T2 | Unknown USB flash drive | Block/ask |
| T3 | Allowlisted keyboard | Allow |
| T4 | HID masquerading as keyboard | CAPTCHA/block |
| T5 | Allowlisted storage + sensitive-file copy | Alert/block |
| T6 | Masquerade storage | Behavior detection |
| T7 | USB Wi-Fi adapter | Detect network device |
| T8 | DNS manipulation in lab | Alert/block |
| T9 | Multiple USB devices | Correct classification |
| T10 | Central server | Logs/alerts displayed |

That gives you a **very convincing project demonstration**.

---

## The feasibility ranking I'd use

```text
                    EASIEST
                       │
                       ▼
             USB data theft
                  ⭐⭐⭐
                       │
             Device allowlist
                  ⭐⭐⭐
                       │
            BadUSB/HID attack
                  ⭐⭐⭐⭐
                       │
        Masquerade storage/Hermes
                  ⭐⭐⭐⭐⭐
                       │
        DNS spoofing via USB NIC
                  ⭐⭐⭐⭐⭐
                       │
             Kernel-level USB
                  ⭐⭐⭐⭐⭐⭐⭐⭐
                       │
                    HARDEST
```

So **yes, you can replicate the important attacks from the paper**. You don't need to reproduce the paper's exact commercial hardware or every attack discussed in its literature review.

For your project, I'd make **BadUSB + data exfiltration the mandatory two attacks**, and **DNS spoofing the third attack if you have enough time**. Those three map directly to the paper's three behavior detectors and give you a very strong end-to-end demonstration.

---

# you asked

message time: 2026-08-26 22:31:18

I can't able to get a USB rubber ducky, I want to do this at low cost as possible. What devices I need to perform this attacks? And how can I implement the attacks using the device , attacks like device spoofing for allowlist bypass, masquerade keyboard, storage attack, network attack

---

# chatgpt response

Yes. You **do not need a Rubber Ducky** to reproduce the important experiments. You can build a low-cost USB attack lab using ordinary development boards and a couple of cheap USB peripherals.

One important distinction: for the **allowlist-bypass/spoofing** experiment, I can help you build a controlled test that demonstrates the weakness of VID/PID/serial allowlisting, but I wouldn't recommend giving instructions for cloning a real device's identifiers to bypass a security system. You can demonstrate the same research point safely with a test device/test harness.

## 1. Cheapest hardware set I recommend

### Minimum shopping list

[Raspberry Pi Pico RP2040-Zero](https://www.google.com/search?tbm=shop&q=Raspberry%20Pi%20Pico%20RP2040-Zero)

![Raspberry Pi Pico RP2040-Zero 1](https://images.openai.com/thumbnails/url/EEeunHicDclZCoJAGADgE7kwhqgQYZJbOWlp24u4jqKO5vwmdqqu023qe_2-nwpgYJogFDQblwGKnIOUIp4wSKDO-KzvBFb1w1BTsnmu_6fpOFetLAySPUOdQpSkGhtZecuGP8s45M4P6Zg2SI5jM7UPjWpPk1g2UWtAOS-vwHIsF0v6lVLzXlKfjN4OoNJJiZFfnwwj0lcsKgrs5DWSEHXf2xu1Rcm7qFMrWtz8A36MPcY)

**₹199 · iFuture Technology · ★ 4.8 (59)**

[Mini USB ATmega32U4 Pro Micro](https://www.google.com/search?tbm=shop&q=Mini%20USB%20ATmega32U4%20Pro%20Micro)

![Mini USB ATmega32U4 Pro Micro 1](https://images.openai.com/thumbnails/url/etnkdnicDclJDoIwFADQEzE2DUJijDgwpSAxamBjoMWCYSjtN-pFvJu30bd9308LIJRnGM1I5VtAwzSoR0vnCiroqE6nwVDtJEQ38tW8_J-3Tpkb0JyIXW1pISU-n--Jo7KjE8cyor2oi_aUReWlKh4bt4eMJZgFTXMOUvXcE1PapYuHG5IkV9hn_G0jfACKenp9LSoZFltk_gCRnDUh)

**₹401.40 · ElectroPi.in · ★ 4.4 (228)**

[EVM Enstore 16GB USB 2.0 Pendrive](https://www.google.com/search?tbm=shop&q=EVM%20Enstore%2016GB%20USB%202.0%20Pendrive)

![EVM Enstore 16GB USB 2.0 Pendrive 1](https://images.openai.com/thumbnails/url/klKToHicDclvD0JAHADgT4QhjK21_uDSREdZvTHuDLc5Jz-ZPlTfp29Tz9vn-2kAxOgoSsXJcxFQUQlKrsv1CAW0RCZ9p4xNL0TL682w_p-zPVPbJwnuluwmFjpUrbBQtEJ0p5axQV4EVq4Z80NhDNWMj8vEDBznLJvuQCT1dQIW8N3sh-fLrIs9vZpJjZgdWlKqIo9pKLfgmb5bDadeQL0Ss0cSun2eT0lEflPYPjk)

**₹320 · Satyam Stationers + others**

[ZEBRONICS ZEB-USB150WF1 WiFi USB Adapter](https://www.google.com/search?tbm=shop&q=ZEBRONICS%20ZEB-USB150WF1%20WiFi%20USB%20Adapter)

![ZEBRONICS ZEB-USB150WF1 WiFi USB Adapter 1](https://images.openai.com/thumbnails/url/zVSK1HicDcndEoFAGADQJ6pEpGaMychPIqUfuWlqK0K1dj-0-1Rex9twbs_3cwHAVJekokGEYShyAbJGFs8UUqiQiNpaopcW46o5Tx-T_-nGLteWyC1vORxRZA0L-bYW4trqmQMHwg3phm_QuoHBuDfTumjLX1VCAu6Xpt3fBORZzqNnUSZZXt8PqepTDQdUMbjHxpyoCMW-uVLs0R6d2Hrh7nuOJdi438amfGVp6P4A5Vc_PQ)

**₹249 · Zebronics + others · ★ 5 (1)**

| Hardware | Approx. cost | Purpose |
|---|---:|---|
| **RP2040-Zero / Raspberry Pi Pico** | ₹200–₹400 | HID attack device |
| **ATmega32U4 Pro Micro** | ₹400–₹500 | Alternative HID device |
| **16 GB USB flash drive** | ₹200–₹500 | Storage attack |
| **USB Wi-Fi adapter** | ₹250–₹700 | Network-device attack |
| Existing Windows PC | ₹0 | USBIPS client |
| Existing router/mobile hotspot | ₹0 | Isolated network lab |

A Raspberry Pi Pico is particularly attractive because its USB stack supports HID and mass-storage/device classes through TinyUSB; Raspberry Pi's own examples include HID and CDC+MSC device examples. ([GitHub](https://github.com/raspberrypi/tinyusb/blob/pico/docs/getting_started.md?utm_source=chatgpt.com))

An ATmega32U4 Pro Micro is also very convenient because the chip has native USB HID capability; current Indian listings show boards around ₹429–₹599. ([AM Electronics](https://amelectronics.in/product/mini-usb-atmega32u4-pro-micro-5v-16mhz-board-modules-2/?utm_source=chatgpt.com))

For the Wi-Fi adapter, don't buy the most expensive one. Current Indian listings show basic USB Wi-Fi adapters from roughly ₹250–₹700, although **Linux driver/chipset compatibility matters if you intend to use Linux for the lab**. ([Smartprix](https://www.smartprix.com/usb_wireless_adapters/price-below_10000?utm_source=chatgpt.com))

### You can realistically keep the hardware under ~₹1,500.

---

# 2. How each attack can be reproduced

I would divide your lab into **four experiments**.

```text
                    USBIPS TEST LAB
                         │
       ┌─────────────────┼─────────────────┐
       │                 │                 │
       ▼                 ▼                 ▼
   Pico/Pro       USB Flash Drive     USB Wi-Fi
       │                 │                 │
       ▼                 ▼                 ▼
     HID             Storage          Network
     Attack           Attack           Attack
       │                 │                 │
       └─────────────────┼─────────────────┘
                         ▼
                    Windows PC
                    + USBIPS
```

---

# 3. Attack 1 — Masquerade keyboard / BadUSB

## Hardware

You only need:

> **Raspberry Pi Pico OR ATmega32U4 Pro Micro**

I would buy the **RP2040-Zero/Pico** because it's inexpensive and TinyUSB already provides HID examples. TinyUSB supports keyboard, mouse, gamepad and composite HID devices. ([TinyUSB](https://docs.tinyusb.org/en/latest/examples/device/hid_composite.html?utm_source=chatgpt.com))

### What happens

Your board is programmed as:

```text
USB device
     ↓
HID keyboard
     ↓
Windows thinks:
"Keyboard connected"
```

Then your firmware can send a **harmless predetermined test sequence**, for example:

```text
T
E
S
T
```

or open Notepad and type:

```text
USBIPS TEST
```

The purpose isn't to damage the computer. You're testing:

> **Can USBIPS distinguish a human keyboard from an automatically generating HID?**

The paper uses exactly this fundamental behavior: its masquerade keyboard automatically sends keystrokes after connection, while USBIPS blocks those keystrokes until the HID passes its verification process. 

### Your experiment

```text
Pico
 │
 │ USB
 ▼
Windows
 │
 ▼
USBIPS
 │
 ├── Detect HID
 │
 ├── Block input
 │
 ├── CAPTCHA
 │
 └── Monitor behavior
```

Then compare:

### Normal keyboard

```text
Human types CAPTCHA
       ↓
CAPTCHA succeeds
       ↓
ALLOW
```

### Pico HID

```text
Pico automatically sends keys
       ↓
Cannot correctly interact with CAPTCHA
       ↓
INPUT BLOCKED
       ↓
ALERT
```

That's enough to reproduce the **security concept** demonstrated by the Rubber Ducky without owning one.

---

# 4. Attack 2 — Device spoofing / allowlist bypass

This is actually the most interesting experiment for your project.

The paper demonstrated that allowlisting can be bypassed when a malicious device presents identifiers matching a legitimate device. The Rubber Ducky in their experiment wasn't blocked by the allowlist because it duplicated identifiers of keyboards already trusted by the host. 

## But don't start by cloning a real device

Instead, make your USBIPS implementation support a **test/simulation mode**.

For example, have your test database contain:

```text
Trusted device

VID       TEST01
PID       KEY01
Serial    KEYBOARD001
Class     HID
```

Then your testing framework generates:

```text
Device A
VID       TEST01
PID       KEY01
Serial    KEYBOARD001
Class     HID
```

Your allowlist says:

```text
MATCH → ALLOW
```

Then simulate the malicious behavior:

```text
Device metadata:
trusted

Behavior:
automated keyboard
```

Your system should produce:

```text
ALLOWLIST:
PASS

BEHAVIOR:
FAIL

FINAL:
BLOCK
```

This demonstrates the exact research point:

> **Identity-based trust ≠ behavioral trust.**

### Better architecture

Make your classifier output:

```cpp
struct USBDevice {
    string vid;
    string pid;
    string serial;
    string deviceClass;
};
```

Then your behavior engine receives:

```text
USBDevice
    +
BehaviorEvents
```

So the allowlist decision and behavior decision remain separate.

---

# 5. If you eventually want actual device-identity testing

You can use a development board whose USB descriptors you control, but **keep this strictly to your own lab devices**.

The important thing for your research is not "I successfully cloned a commercial keyboard."

The important result is:

```text
             IDENTITY LAYER
                   ↓
              Trusted ✓
                   ↓
          passes allowlist
                   ↓
             BEHAVIOR LAYER
                   ↓
        Automated HID behavior ✗
                   ↓
                BLOCK
```

That is much more valuable academically.

---

# 6. Attack 3 — USB storage / data theft

This one is extremely cheap.

You need:

> **Any ordinary USB flash drive**

Even a ₹200–₹400 drive is sufficient.



Create a test directory:

```text
C:\USBIPS-Lab\Confidential\
```

Put harmless dummy data inside:

```text
employee_data.txt
project_design.pdf
passwords_demo.txt
```

Then connect your USB:

```text
USB drive
    ↓
USBIPS detects STORAGE
    ↓
Allowlist check
    ↓
Mount
    ↓
Copy:
C:\USBIPS-Lab\Confidential\*
            ↓
        USB drive
```

USBIPS should detect:

```text
READ:
C:\USBIPS-Lab\Confidential\secret.txt

WRITE:
E:\secret.txt
```

and generate:

```text
╔══════════════════════════════╗
║ 🚨 DATA EXFILTRATION         ║
╠══════════════════════════════╣
║ Source: Confidential folder  ║
║ Destination: USB             ║
║ Process: explorer.exe        ║
║ Device: USB001               ║
║ Severity: HIGH               ║
║ Action: BLOCK                ║
╚══════════════════════════════╝
```

This is almost exactly the experiment described in the paper. 

### You don't need Hermes for this.

For your project:

> **Normal flash drive + controlled sensitive files = sufficient storage attack demonstration.**

---

# 7. Attack 4 — Network attack

Buy:

> **USB Wi-Fi adapter**



However, **choose one with known Linux support if you're going to use Linux for the network-side test**. Linux wireless documentation notes that hardware/driver capabilities vary, so don't choose purely based on price. ([Linux Wireless Documentation](https://wireless.docs.kernel.org/en/latest/en/users/documentation/modes.html?utm_source=chatgpt.com))

You don't actually need an expensive Wi-Fi adapter.

The paper's attack is:

```text
USB Wi-Fi adapter
       ↓
Network interface
       ↓
DNS configuration/traffic manipulation
       ↓
DNS response becomes abnormal
       ↓
USBIPS detects
```

The paper created a test DNS server in the same network and observed redirection to a malicious destination. 

---

# 8. Do the network experiment safely

I recommend:

```text
              ISOLATED LAB
                   
        ┌─────────────────┐
        │ Test Router/AP  │
        └────────┬────────┘
                 │
       ┌─────────┴─────────┐
       │                   │
       ▼                   ▼
 Windows USBIPS       Linux VM
      PC             Test DNS server
```

Use a fake test domain such as:

```text
usbips.test
```

Expected:

```text
usbips.test → 192.168.50.20
```

Your test DNS service can intentionally return:

```text
usbips.test → 192.168.50.99
```

Then USBIPS detects:

```text
Expected DNS result:
192.168.50.20

Observed:
192.168.50.99

        ↓

DNS ANOMALY
        ↓
ALERT
```

You don't need to attack Google, a real website, or anyone else's network.

---

# 9. What I would buy

If your goal is **minimum possible cost**, I'd buy only these:

### Device 1 — RP2040-Zero/Pico

For:

- HID keyboard
- HID masquerade
- automated input
- potentially composite HID experiments

Current listings show RP2040 boards around ₹200–₹400. ([Yantronix](https://www.yantronix.in/products/raspberry-pi-pico---original/1414116000000645368?utm_source=chatgpt.com))

### Device 2 — USB flash drive

For:

- storage detection
- data exfiltration
- file monitoring

### Device 3 — USB Wi-Fi adapter

For:

- network device classification
- USB NIC detection
- DNS/DHCP behavior experiment

### That's it.

You **do not need**:

❌ Rubber Ducky  
❌ Hak5 gear  
❌ expensive USB protocol analyzer  
❌ Raspberry Pi 5  
❌ dedicated server  
❌ expensive Wi-Fi adapter  
❌ multiple laptops  

---

# 10. You can even use ONE Pico for several experiments

This is the really useful part.

A Pico can expose different USB device configurations using TinyUSB. TinyUSB supports HID and Mass Storage among its device classes. ([TinyUSB](https://docs.tinyusb.org/en/latest/?utm_source=chatgpt.com))

So your test board can have modes:

```text
              RP2040
                 │
        ┌────────┼────────┐
        ↓        ↓        ↓
     HID mode  MSC mode  Composite
        │        │          │
     Keyboard  Storage    HID + MSC
```

For example:

### Mode 1

```text
Pico → HID keyboard
```

### Mode 2

```text
Pico → Mass Storage
```

### Mode 3

```text
Pico → HID + Mass Storage
```

TinyUSB even provides a `cdc_msc` example and HID composite examples, so you aren't starting the USB firmware stack from scratch. ([GitHub](https://github.com/raspberrypi/tinyusb/blob/pico/docs/getting_started.md?utm_source=chatgpt.com))

**This is potentially much better for your project than buying a Rubber Ducky.**

---

# 11. Your complete low-cost attack lab

```text
                         YOUR PC
                    Windows 11 + USBIPS
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
        │                  │                  │
        ▼                  ▼                  ▼
   ┌──────────┐       ┌──────────┐      ┌──────────┐
   │ RP2040   │       │ USB      │      │ USB WiFi │
   │ Pico     │       │ Flash    │      │ Adapter  │
   │          │       │ Drive    │      │          │
   └────┬─────┘       └────┬─────┘      └────┬─────┘
        │                  │                  │
        ▼                  ▼                  ▼
      HID               Storage            Network
        │                  │                  │
        ▼                  ▼                  ▼
  Masquerade           Data theft       DNS behavior
   keyboard             detection         detection
```

---

# 12. And your demonstrations become very clear

### Demonstration 1 — Allowlist alone fails

```text
Test HID
   ↓
Trusted-looking identity
   ↓
ALLOWLIST PASS
   ↓
Automated behavior
   ↓
BEHAVIOR DETECTOR
   ↓
BLOCK
```

### Demonstration 2 — BadUSB/HID

```text
Pico
 ↓
Enumerates as keyboard
 ↓
Automatically sends harmless test keystrokes
 ↓
USBIPS CAPTCHA
 ↓
Automated device cannot satisfy human verification
 ↓
BLOCK
```

### Demonstration 3 — Data theft

```text
USB storage
 ↓
Access protected test file
 ↓
Copy to USB
 ↓
USBIPS detects source + destination
 ↓
BLOCK + LOG
```

### Demonstration 4 — Network

```text
USB Wi-Fi
 ↓
New network interface
 ↓
Baseline DNS
 ↓
Controlled DNS change
 ↓
Unexpected resolution
 ↓
USBIPS ALERT
```

---

## My strongest recommendation

**Buy the RP2040-Zero/Pico first.**

It's the single most useful piece of hardware for this project. TinyUSB gives you the USB HID functionality you need, and the Raspberry Pi Pico SDK has USB device examples ready to build. ([GitHub](https://github.com/raspberrypi/tinyusb/blob/pico/docs/getting_started.md?utm_source=chatgpt.com))

Then get:

**Pico + ₹300–₹500 flash drive + ₹300–₹700 Wi-Fi adapter.**

That gives you enough hardware to reproduce the **three major attack categories in the paper**, without spending money on a Rubber Ducky.

And I would design the project so that **the attack devices are deliberately simple**, while your USBIPS software is the sophisticated part. That makes your final project much more defensible: you're demonstrating that your system detects *behavior*, rather than merely showing that you bought specialized attack hardware.

---

# you asked

message time: 2026-08-26 22:48:46

Separate the project into modules and give a development plan. I think of implementing the allowlist system for the first phase completing both client and server side. What's your thought?

---

# chatgpt response

Yes — **I strongly agree with your Phase 1 idea**. In fact, I think it is the best way to start this project.

Instead of building the client completely and then the server completely, I recommend building the **first end-to-end vertical slice**:

> **USB detected → device information extracted → allowlist checked → decision made → device allowed/blocked → event logged on server.**

That gives you a working USBIPS foundation before you touch the much harder behavioral detection modules.

The paper itself separates the system into a **USBIPS client** and **USBIPS server**, with the client responsible for device classification/access control and the server responsible for centralized allowlist management and logging. 

---

# 1. Overall project modules

I would divide your project into **7 major modules**:

```text
                         USBIPS
                           │
          ┌────────────────┴────────────────┐
          │                                 │
      USB CLIENT                       USB SERVER
          │                                 │
 ┌────────┼─────────┐              ┌────────┼──────────┐
 │        │         │              │        │          │
 ▼        ▼         ▼              ▼        ▼          ▼
Device   Access   Behavior       Client   Allowlist   Log
Monitor  Control  Detection      Monitor  Manager    Analyzer
                    │
             ┌──────┼──────┐
             │      │      │
             ▼      ▼      ▼
            HID  Storage Network
```

More specifically:

## Client modules

### C1. USB Device Monitor
Detect:

- USB insertion
- USB removal
- device arrival/removal events

### C2. Device Information Extractor
Collect:

- VID
- PID
- Serial Number
- Device class/type
- Device description
- Drive information
- Volume serial
- Filesystem
- Device path/key

The paper's device structure explicitly includes these kinds of identifiers and properties. 

### C3. Device Classifier

Classify:

```text
USB
 ├── HID
 ├── Storage
 ├── Network
 └── Other
```

### C4. Allowlist Access Controller

```text
Device information
        ↓
Local allowlist
        ↓
MATCH ───────→ ALLOW
NO MATCH
   ↓
User decision
   ├── Approve → Add allowlist → ALLOW
   └── Reject  → BLOCK
```

This follows the paper's access-controller design. 

### C5. Behavior Detection Engine

Later:

```text
Behavior Engine
 ├── HID Detector
 ├── Storage Detector
 └── Network Detector
```

### C6. Client Logger

Records:

- Device connected
- Device removed
- Allowlist match
- Allowlist rejection
- User approval
- Device blocked
- Behavior alert

### C7. Windows Service / Daemon

Keeps the client running in the background.

---

# 2. Server modules

The paper describes four server-side functions:

```text
USBIPS SERVER

1. Client Status Monitor
2. Log Analyzer
3. Allowlist Manager
4. Behavior Rule Manager
```



For your implementation:

### S1. REST API

Communication:

```text
Client ←──── REST/JSON ────→ Server
```

The paper explicitly uses REST and JSON for client-server communication. 

### S2. Allowlist Manager

Admin can:

- View devices
- Add device
- Remove device
- Approve device
- Revoke device
- Search devices

### S3. Client Manager

Show:

```text
Client ID
Hostname
IP
Status
Last heartbeat
USBIPS version
```

### S4. Log Analyzer

Display:

```text
Device connected
Device allowed
Device blocked
Device removed
```

### S5. Behavior Rule Manager

**Build later.**

It will eventually distribute:

```text
HID rules
Storage rules
Network rules
```

---

# 3. Your Phase 1 idea

I would make Phase 1 specifically:

# **Phase 1 — Complete Allowlist System**

And importantly:

> **Build both client AND server in Phase 1.**

Don't build only the server database or only the Windows client.

Build the smallest complete end-to-end system.

---

# 4. Phase 1 architecture

```text
                    USBIPS SERVER
             ┌────────────────────────┐
             │                        │
             │   FastAPI REST API     │
             │                        │
             │  Allowlist Manager     │
             │  Client Manager        │
             │  Log API               │
             │                        │
             └───────────┬────────────┘
                         │
                    REST / JSON
                         │
                         ▼
                 USBIPS WINDOWS CLIENT
             ┌────────────────────────┐
             │                        │
USB ───────→ │ Device Monitor         │
             │       ↓                │
             │ Device Information     │
             │       ↓                │
             │ Classifier             │
             │       ↓                │
             │ Allowlist Controller   │
             │       ↓                │
             │ ALLOW / BLOCK          │
             │       ↓                │
             │ Logger                 │
             └────────────────────────┘
```

---

# 5. Development plan

I would develop it in this order.

## Phase 0 — Environment setup

Before writing functionality:

### Client

- Visual Studio
- C++
- Windows SDK
- Git
- CMake or Visual Studio project

### Server

- Python
- FastAPI
- Uvicorn
- SQLite
- SQLAlchemy/SQLModel
- Git

### Testing

- Windows 11 PC
- 2–3 USB devices

Don't start with the attack hardware yet.

---

# Phase 1A — USB detection

**Goal:** Detect when a USB device is inserted/removed.

Implement:

```text
USBDeviceMonitor
```

Expected output:

```text
[USB INSERTED]

Device detected.
```

and:

```text
[USB REMOVED]

Device disconnected.
```

The paper's client uses Windows API/device notification mechanisms to detect device connections and then passes the information to the classifier. 

### Deliverable

A C++ program that continuously runs:

```text
Waiting for USB...

USB detected!
USB removed!
```

---

# Phase 1B — Device information extraction

Now extend the detector.

For every device:

```text
Device
 ├── VID
 ├── PID
 ├── Serial
 ├── Device Class
 ├── Description
 ├── Device Path
 └── Drive/Volume information
```

Example:

```text
================================
USB DEVICE
================================
VID        : 0951
PID        : 1666
Serial     : ABC123456
Class      : Mass Storage
Description: Kingston DataTraveler
Drive      : E:
Volume     : USB_DRIVE
================================
```

### Deliverable

A reliable `USBDevice` C++ structure.

---

# Phase 1C — Device classification

Implement:

```cpp
enum class DeviceType {
    HID,
    STORAGE,
    NETWORK,
    OTHER
};
```

Then:

```text
USB device
    ↓
Classifier
    ↓
HID / STORAGE / NETWORK / OTHER
```

### Deliverable

Your client can correctly classify the devices you connect.

---

# Phase 1D — Local allowlist

**Do this before connecting the server.**

Create a local SQLite database:

```text
devices
--------------------------------
id
vid
pid
serial
device_type
description
volume_serial
status
created_at
```

Then implement:

```text
Device detected
      ↓
Extract identity
      ↓
Query SQLite
      ↓
MATCH?
```

### Deliverable

You should be able to demonstrate:

```text
USB A → allowlisted → ALLOWED
USB B → unknown     → BLOCK/ASK
```

---

# Phase 1E — Access controller

Now connect the allowlist to the actual decision.

Your logic:

```text
             DEVICE
                ↓
          Get identity
                ↓
       Search local allowlist
                ↓
          ┌─────┴─────┐
        MATCH       NO MATCH
          ↓             ↓
        ALLOW       Ask user
                       ↓
                ┌──────┴──────┐
               YES            NO
                ↓              ↓
             ALLOW           BLOCK
                ↓
          Add to allowlist
```

This is the core access-control mechanism described by the paper. 

### Important

Don't worry about sophisticated behavior detection yet.

At this point you should be able to say:

> **"My USBIPS can identify a USB device and enforce an allowlist policy."**

---

# Phase 1F — Build the server

Now start your FastAPI server.

Basic endpoints:

```text
GET    /api/devices
POST   /api/devices
DELETE /api/devices/{id}

GET    /api/clients
POST   /api/clients/register

POST   /api/events
GET    /api/events
```

You can later expand these.

---

# Phase 1G — Connect client ↔ server

This is where your project starts looking like the paper.

When the USB device is detected:

```text
CLIENT

USB detected
    ↓
Extract information
    ↓
POST /api/device/check
    ↓
SERVER
    ↓
Allowlist database
    ↓
Decision
    ↓
JSON response
    ↓
CLIENT
    ↓
ALLOW / BLOCK
```

Example request:

```json
{
    "vid": "0951",
    "pid": "1666",
    "serial": "ABC123",
    "device_type": "STORAGE"
}
```

Response:

```json
{
    "allowed": true,
    "reason": "ALLOWLIST_MATCH"
}
```

---

# 6. But I recommend one important modification

Don't make the server the **only** place where the allowlist exists.

Use:

```text
                  SERVER
              Master Allowlist
                    │
              Sync / Update
                    │
                    ▼
                  CLIENT
              Local Allowlist
                    │
                    ▼
              Real-time decision
```

Why?

USB devices must be controlled immediately. You don't want:

```text
USB connected
     ↓
Internet/server unavailable
     ↓
USBIPS cannot decide
```

The paper explicitly states that USBIPS clients can operate online and offline, while the server provides centralized management. 

So your client should maintain a **local cached allowlist**.

---

# 7. Phase 1 final functionality

By the end of Phase 1, you should have this:

```text
                   ADMIN DASHBOARD
                         │
                         ▼
                  Allowlist DB
                         │
                    REST API
                         │
                         ▼
                  USBIPS CLIENT
                         │
                  USB connected
                         │
                         ▼
                 Device Detection
                         │
                         ▼
                 Device Classifier
                         │
                         ▼
                 Local Allowlist
                         │
              ┌──────────┴──────────┐
              ▼                     ▼
           TRUSTED                UNKNOWN
              │                     │
           ALLOW              User decision
                                    │
                             ┌──────┴──────┐
                             ▼             ▼
                           ALLOW         BLOCK
                             │             │
                             └──────┬──────┘
                                    ▼
                                  LOG
                                    │
                                    ▼
                              SERVER LOG
```

That is a **complete working Phase 1**.

---

# 8. Phase 2 — Central management

Once Phase 1 works, improve the server.

Implement:

### Client registration

```text
Client 001
Hostname: COSMI-PC
IP: 192.168.1.10
Status: ONLINE
Last heartbeat: 22:15
```

### Allowlist synchronization

```text
Server
  ↓
Updated allowlist
  ↓
Client
  ↓
Local database updated
```

### Remote actions

Admin:

```text
[BLOCK DEVICE]
[ALLOW DEVICE]
[REMOVE FROM ALLOWLIST]
```

This corresponds to the paper's centralized allowlist management. 

---

# 9. Phase 3 — Logging and dashboard

Build the security dashboard.

Something like:

```text
┌───────────────────────────────────────────┐
│              USBIPS DASHBOARD             │
├───────────────────────────────────────────┤
│                                           │
│ Clients             3 Online              │
│ USB Devices         12                    │
│ Allowed             9                     │
│ Blocked             3                     │
│ Alerts              2                     │
│                                           │
├───────────────────────────────────────────┤
│ Recent Events                              │
│                                           │
│ 22:10 USB Storage     ALLOWED              │
│ 22:13 Unknown HID     BLOCKED              │
│ 22:14 USB WiFi        ALLOWED              │
│ 22:15 Unknown USB     BLOCKED              │
└───────────────────────────────────────────┘
```

---

# 10. Phase 4 — HID behavior detector

Only after allowlisting is solid.

Implement:

```text
HID connected
     ↓
Is allowlisted?
     ↓
Yes
 ↓
Behavior monitoring
     ↓
Automated keyboard behavior?
     ↓
CAPTCHA / verification
     ↓
ALLOW / BLOCK
```

This is where your Pico becomes useful.

Your first attack test:

```text
Normal keyboard
     VS
Pico HID
```

The paper's HID detector uses an active CAPTCHA-based interaction and blocks keyboard input while verification is pending. 

---

# 11. Phase 5 — Storage behavior detector

Then:

```text
USB Storage
     ↓
File monitoring
     ↓
Sensitive path accessed?
     ↓
        YES
         ↓
Source = protected directory
Destination = USB
         ↓
DATA EXFILTRATION
         ↓
BLOCK + ALERT
```

The paper demonstrated exactly this type of data-theft detection using file activity records. 

---

# 12. Phase 6 — Network behavior detector

Finally:

```text
USB Network Adapter
        ↓
Network configuration baseline
        ↓
Monitor
 ├── DNS
 ├── Gateway
 └── DHCP
        ↓
Abnormal change?
        ↓
       YES
        ↓
Alert / block / restore
```

Then reproduce the paper's DNS-spoofing experiment inside your isolated test network. 

---

# 13. Phase 7 — Attack evaluation

Only **after your defense is working** should you start your attack experiments.

I'd structure the evaluation as:

### Test A — Normal device

```text
Allowlisted USB
→ ALLOW
```

### Test B — Unknown device

```text
Unknown USB
→ ASK
→ BLOCK
```

### Test C — Allowlist bypass scenario

```text
Trusted identity
+
Suspicious behavior
→ BEHAVIOR DETECTOR
→ BLOCK
```

### Test D — Masquerade keyboard

```text
Pico HID
→ HID detector
→ CAPTCHA
→ Automated input blocked
```

### Test E — Data theft

```text
Protected file
→ USB
→ Storage detector
→ ALERT/BLOCK
```

### Test F — Network

```text
USB Wi-Fi
→ DNS anomaly
→ ALERT/BLOCK
```

---

# 14. Development roadmap

I'd use this exact sequence:

| Phase | Module | Result |
|---|---|---|
| **0** | Environment setup | Development environment ready |
| **1A** | USB Device Monitor | Detect plug/unplug |
| **1B** | Device Information | VID/PID/Serial etc. |
| **1C** | Classifier | HID/Storage/Network |
| **1D** | Local Allowlist DB | Store trusted devices |
| **1E** | Access Controller | Allow/block devices |
| **1F** | REST Server | Server API |
| **1G** | Client ↔ Server | End-to-end allowlisting |
| **2** | Client management | Multiple clients |
| **3** | Logging/dashboard | Central monitoring |
| **4** | HID detector | BadUSB defense |
| **5** | Storage detector | Data-theft defense |
| **6** | Network detector | DNS/network defense |
| **7** | Attack testing | Controlled attacks |
| **8** | Performance evaluation | CPU/RAM/I/O/latency |
| **9** | Final integration | Complete USBIPS |

---

# 15. Why I think your Phase 1 approach is particularly good

There are three reasons.

### 1. It gives you a working product early

After Phase 1, you already have:

> **A functional Windows USB access-control system with centralized management.**

You're not spending months on behavior detection before having anything demonstrable.

### 2. Every later module plugs into the same architecture

Your future pipeline becomes:

```text
USB Event
   ↓
Device Information
   ↓
Classification
   ↓
Allowlist
   ↓
Behavior Engine
   ↓
Decision
   ↓
Logger
   ↓
Server
```

So you're building the foundation in the correct order.

### 3. It directly follows the base paper

The paper itself places **device classification → allowlisting → behavior detection** in that order. The behavior detector starts monitoring after a device has passed classification/allowlisting. 

---

## One change I'd make to your Phase 1

Don't call it simply **"Allowlist System."**

Call it:

> **Phase 1 — USB Device Identification, Classification & Allowlist Access Control**

Because by the end of the phase, you should have all three:

```text
USB Detection
      ↓
Device Identification
      ↓
Device Classification
      ↓
Allowlist
      ↓
Access Decision
      ↓
Logging
      ↓
Server
```

That gives you a much stronger milestone.

**And I would not start buying the attack hardware yet.** First get Phase 1 working with your normal keyboard, mouse, flash drive, and Wi-Fi adapter. Once the client can reliably identify and classify those devices, then buy the inexpensive Pico and use it specifically for Phase 4's HID attack testing.

---

# you asked

message time: 2026-08-26 23:04:25

At the end of the phase one, does the identification, classification and access control is running as a service in windows or just a C++ program running in the system ?

---

# chatgpt response

At the end of **Phase 1, I recommend that it should be a Windows Service**, not just a C++ program that you manually run.

The distinction is important because your project is supposed to function as an actual **USB security/IPS client**.

### Phase 1 final architecture

```text
                 WINDOWS 11
┌───────────────────────────────────────────────┐
│                                               │
│          USBIPS Windows Service               │
│                                               │
│   ┌───────────────────────────────────────┐   │
│   │ USB Device Monitor                    │   │
│   │          ↓                            │   │
│   │ Device Identification                 │   │
│   │          ↓                            │   │
│   │ Device Classification                 │   │
│   │          ↓                            │   │
│   │ Allowlist Access Controller            │   │
│   │          ↓                            │   │
│   │ ALLOW / BLOCK                          │   │
│   │          ↓                            │   │
│   │ Event Logger                           │   │
│   └───────────────────────────────────────┘   │
│                    │                          │
│                    │ REST / JSON              │
│                    ▼                          │
│             USBIPS SERVER                    │
│                                               │
└───────────────────────────────────────────────┘
```

The paper itself describes the **USBIPS service observer as a Windows system service that automatically activates when Windows starts**, while the daemon handles communication with the kernel modules and activates the other client modules. 

So for your project, I would follow that overall architecture, but **simplify the self-protection mechanism** initially.

---

## How we should develop it

Don't try to write the Windows Service first.

Build it in stages:

### Step 1 — Normal C++ program

First:

```text
USBMonitor.exe
```

It runs manually and does:

```text
Detect USB
   ↓
Extract VID/PID/Serial
   ↓
Classify
   ↓
Check allowlist
   ↓
Allow/Block
```

This makes debugging much easier.

---

### Step 2 — Add local database

```text
USBMonitor.exe
      ↓
SQLite
      ↓
Allowlist
```

Now verify that:

```text
Known USB → ALLOW
Unknown USB → BLOCK/ASK
```

works reliably.

---

### Step 3 — Connect your server

```text
USBMonitor.exe
      │
      │ REST/JSON
      ▼
FastAPI Server
      │
      ▼
Allowlist DB
```

The paper specifies REST + JSON for client/server communication. 

---

### Step 4 — Convert the C++ program into a Windows Service

Then your executable becomes something like:

```text
USBIPSClient.exe
```

registered as:

```text
USBIPS Client Service
```

and configured:

```text
Startup type: Automatic
```

So when you boot Windows:

```text
Windows starts
      ↓
USBIPS Client Service starts
      ↓
USB monitoring begins
      ↓
Wait for USB events
```

You don't have to manually open a program.

---

# Why this matters

Imagine your computer is currently running:

```text
Windows
   ↓
USBIPS.exe ← manually launched
```

You plug in a malicious USB **before launching USBIPS**.

Your protection isn't active.

That's not a good security product.

Instead:

```text
Windows boot
     ↓
USBIPS Service starts automatically
     ↓
USB monitoring always active
```

That's much closer to what you actually want.

---

# One important architectural point

I wouldn't put **everything into one giant Windows Service executable**.

Eventually, I would structure your client approximately like:

```text
USBIPS Client
│
├── USBIPSService.exe
│       │
│       ├── DeviceMonitor
│       ├── DeviceIdentifier
│       ├── DeviceClassifier
│       ├── AccessController
│       └── Logger
│
├── USBIPSClient.db
│
└── USBIPSTray.exe
        │
        └── User notifications/UI
```

The **service** handles security-critical background operations.

The **tray/UI application** handles things such as:

```text
Unknown USB detected

Device: Kingston DataTraveler
VID: 0951
PID: 1666

[Allow] [Block]
```

This separation becomes especially useful later when you implement the HID CAPTCHA and user interaction.

---

# So your Phase 1 milestone should be

By the end of Phase 1, you should be able to:

1. Start Windows.
2. USBIPS service automatically starts.
3. Plug in a USB device.
4. Service detects it.
5. Extracts VID/PID/Serial/etc.
6. Classifies it.
7. Checks the local allowlist.
8. Allows or blocks it.
9. Records the event.
10. Synchronizes/communicates with your server.
11. Server dashboard shows the event.

So **yes: the final Phase 1 implementation should be a Windows Service**, but **we develop and debug the exact same functionality first as a normal C++ application and convert it to a service near the end of Phase 1**.

That will save you a lot of debugging pain.

---

# you asked

message time: 2026-08-26 23:13:53

Give the complete phase 1 development plan clearly as a docx file

---

# chatgpt response

Done. I created the complete **Phase 1 development plan** as a DOCX, covering:

- Architecture
- Client and server modules
- Technology stack
- Detailed development steps
- Local + centralized allowlist design
- REST API flow
- SQLite data models
- Windows Service conversion
- Testing plan
- Completion criteria
- Project folder structure
- Final Phase 1 demonstration
- Phase 1 deliverables

[**Download the USBIPS Phase 1 Development Plan (.docx)**](sandbox:/mnt/data/USBIPS_Phase_1_Development_Plan.docx)

---

# you asked

message time: 2026-08-28 13:02:02

What are the variables does the USB device structure has? and what each variable for? How the extraction happens? Give the flow from connecting USB to detection,extraction each calls

---

# chatgpt response

Yes. Since you're now implementing **Phase 1**, it's important to understand exactly what the `CDevice` object represents and how Windows gets each value.

I checked the paper's actual `CDevice` definition and workflow. The paper defines **13 data fields** plus four helper methods. 

One clarification first: **the paper does not provide the exact internal implementation/code for every extraction call**. It explains the workflow and the `CDevice` structure, but not every Windows API used to populate each field. So below I'll distinguish **what the paper says** from the **Windows implementation we should use for your project**.

---

# 1. The `CDevice` structure in the paper

The paper defines:

```cpp
class CDevice {
private:
    UINT DeviceType;
    STORAGE_BUS_TYPE DeviceBus;
    TCHAR VendorID[64];
    TCHAR ProductID[64];
    TCHAR ProductRev[64];
    TCHAR SerialNumber[64];
    TCHAR DeviceKey[256];

public:
    TCHAR Drive[32];
    TCHAR DeviceSN[512];
    TCHAR DeviceDesc[512];
    TCHAR DeviceVolName[64];
    TCHAR DeviceVolSerial[32];
    TCHAR DeviceVolFS[10];

    CDevice(TCHAR *DriveLetter);

    bool IsUSBDevice();
    bool IsIEEE1394Device();
    bool IsCDROM();
    bool IsNoDetect();
};
```



Let's understand each one.

---

# 2. `DeviceType`

```cpp
UINT DeviceType;
```

### What it means

The general type/category of the device.

For your implementation, we should make this more explicit:

```cpp
enum class DeviceType {
    HID,
    STORAGE,
    NETWORK,
    OTHER
};
```

Examples:

```text
Keyboard       → HID
Mouse          → HID
USB flash drive → STORAGE
USB Wi-Fi      → NETWORK
Unknown USB    → OTHER
```

### Where does it come from?

The classification process looks at the device's Windows device/interface information and descriptors.

The paper says the classifier obtains descriptor information and decodes it into readable device information, which is then used to identify whether the device is HID, storage, network, etc. 

---

# 3. `DeviceBus`

```cpp
STORAGE_BUS_TYPE DeviceBus;
```

### What it means

The underlying bus/interface type used by the device.

For example, Windows can report bus types such as:

```text
USB
SATA
SCSI
NVMe
...
```

For USBIPS, this helps distinguish whether the storage device is actually attached through USB.

### Why useful?

Suppose Windows reports:

```text
Device = Storage
Bus = USB
```

Then:

```text
USB STORAGE
```

is relevant to USBIPS.

Whereas:

```text
Device = Storage
Bus = SATA
```

is an internal disk and isn't the removable USB device you're trying to control.

The paper specifically includes `STORAGE_BUS_TYPE` in `CDevice`. 

---

# 4. `VendorID`

```cpp
TCHAR VendorID[64];
```

This is essentially the **VID**.

Example:

```text
Vendor ID = 0951
```

VID identifies the USB vendor/manufacturer.

For example:

```text
VID = 046D
```

could correspond to a Logitech device.

### Why USBIPS needs it

It becomes part of the device identity:

```text
VID + PID + Serial
```

The paper explicitly says VID is one of the pieces of information used by the allowlist. 

---

# 5. `ProductID`

```cpp
TCHAR ProductID[64];
```

This is the **PID**.

Example:

```text
PID = C52B
```

Together:

```text
VID = 046D
PID = C52B
```

identify a particular USB product/model under that vendor.

### Allowlist

Your database might therefore contain:

```text
VID       PID
046D      C52B
```

But we shouldn't rely on only these two values for trust.

---

# 6. `ProductRev`

```cpp
TCHAR ProductRev[64];
```

This is the product/device revision.

Example:

```text
Product Revision = 0110
```

This can distinguish revisions of a product.

It can be useful for:

- device identification
- allowlisting
- logging
- detecting unexpected device changes

The paper includes it directly in `CDevice`. 

---

# 7. `SerialNumber`

```cpp
TCHAR SerialNumber[64];
```

This is the **USB device's serial number**, when the device exposes one.

Example:

```text
SerialNumber = 4C530001230101234567
```

This is considerably more useful than VID/PID alone.

Why?

Imagine:

```text
VID = 1234
PID = 5678
```

Two physical devices can potentially have the same VID/PID.

But:

```text
VID + PID + Serial
```

can distinguish individual devices.

The paper explicitly mentions serial number as an allowlist field. 

---

# 8. `DeviceKey`

```cpp
TCHAR DeviceKey[256];
```

This represents a Windows device-identification/key value used internally to identify the device.

Think of it as a **Windows device identity/path/key**, rather than a simple USB descriptor like VID.

For example, Windows may expose identifiers resembling:

```text
USB\VID_1234&PID_5678\ABC123
```

The exact form depends on the device and Windows device enumeration.

### Important

For your implementation, don't assume:

```text
DeviceKey == Serial Number
```

They are different concepts.

---

# 9. `Drive`

```cpp
TCHAR Drive[32];
```

This represents the drive letter associated with the device.

Example:

```text
E:
```

For a USB flash drive:

```text
USB Storage
    ↓
Windows mounts it
    ↓
E:
```

This becomes particularly important for the storage behavior detector later.

---

# 10. `DeviceSN`

```cpp
TCHAR DeviceSN[512];
```

This is confusing because there is already:

```cpp
SerialNumber
```

The paper doesn't provide enough implementation detail to conclusively explain why it has both fields or exactly how `DeviceSN` differs internally.

So I would **not blindly reproduce this field** in our first implementation.

We can normalize the concept to:

```cpp
std::wstring serialNumber;
```

and keep one canonical serial number.

---

# 11. `DeviceDesc`

```cpp
TCHAR DeviceDesc[512];
```

Human-readable device description.

Example:

```text
Kingston DataTraveler 3.0 USB Device
```

or:

```text
USB Composite Device
```

Useful for:

- logs
- dashboard
- user confirmation

For example:

```text
Unknown USB Device

Description:
Kingston DataTraveler 3.0

VID: 0951
PID: 1666

[Allow] [Block]
```

---

# 12. `DeviceVolName`

```cpp
TCHAR DeviceVolName[64];
```

The filesystem volume label.

Example:

```text
COSMI_USB
```

If you format a flash drive and give it the label:

```text
PROJECT_DATA
```

then:

```text
DeviceVolName = PROJECT_DATA
```

Useful for identifying/logging storage volumes.

---

# 13. `DeviceVolSerial`

```cpp
TCHAR DeviceVolSerial[32];
```

This is the **volume serial number**, which is different from the physical USB device serial.

This distinction is extremely important.

### Physical device

```text
USB Serial Number
       ↓
Device identity
```

### Filesystem

```text
Volume Serial Number
       ↓
Filesystem/volume identity
```

The paper specifically mentions partition/volume information as part of its allowlisting. 

For example:

```text
USB Serial:
ABC123456

Volume Serial:
7A3F-21BC
```

Don't treat these as the same identifier.

---

# 14. `DeviceVolFS`

```cpp
TCHAR DeviceVolFS[10];
```

Filesystem type.

For example:

```text
NTFS
FAT32
exFAT
```

Useful for storage identification and logging.

---

# 15. The four helper methods

The class also has:

```cpp
bool IsUSBDevice();
bool IsIEEE1394Device();
bool IsCDROM();
bool IsNoDetect();
```

These aren't device variables.

They are helper functions used to determine device characteristics.

For your version, we can simplify this substantially.

---

# 16. Now the important part: HOW does extraction actually happen?

This is the full pipeline.

The paper's conceptual flow is:

```text
USB physically connected
          ↓
Windows detects hardware change
          ↓
USBIPS daemon receives notification
          ↓
Device descriptor information obtained
          ↓
USBIPS classifier decodes information
          ↓
CDevice object created
          ↓
Device classification
          ↓
Allowlist comparison
```

This is directly described by the paper. 

Now let's translate that into the actual Windows/C++ development flow we should implement.

---

# 17. Step 1 — USBIPS Service starts

Eventually your program will be:

```text
USBIPSClientService.exe
```

Windows starts it automatically.

Your service initializes:

```text
USB monitoring
↓
Device notification
↓
Database
↓
Server connection
```

---

# 18. Step 2 — Register for device notifications

At startup:

```cpp
RegisterDeviceNotification(...)
```

The paper explicitly states that it calls `RegisterDeviceNotification` during USBIPS daemon startup. 

It registers for interfaces including:

```text
HID
Storage
Network
Other USB devices
```

The paper lists these GUID categories. 

Conceptually:

```text
USBIPS starts
     ↓
RegisterDeviceNotification()
     ↓
Windows knows:
"Tell USBIPS when these device types change."
```

---

# 19. Step 3 — User plugs in USB

Physical event:

```text
             USB
              │
              ▼
        Windows USB stack
              │
              ▼
       Plug and Play manager
              │
              ▼
        Device interface
              │
              ▼
       Device notification
```

USBIPS receives the notification.

The paper describes this as the daemon receiving a notification of a hardware configuration change matching a predefined device class. 

---

# 20. Step 4 — Windows sends the device interface information

The notification contains information that allows the application to identify the relevant device interface.

Conceptually:

```text
WM_DEVICECHANGE
       ↓
DBT_DEVICEARRIVAL
       ↓
DEV_BROADCAST_DEVICEINTERFACE
       ↓
Device interface path
```

This is the point where our code learns:

> **"A particular device interface has arrived."**

It doesn't yet mean we have every field in `CDevice`.

---

# 21. Step 5 — Open/enumerate the device information

Now your C++ code uses Windows device-management APIs to query the device.

Conceptually:

```text
Device interface path
        ↓
Windows device enumeration
        ↓
Device information set
        ↓
Device properties
```

For implementation, this is where we will use Windows **SetupAPI / Configuration Manager / device-property APIs**.

The paper describes this generically as retrieving device information from the Windows device-management structure; it does not specify every individual SetupAPI call. 

---

# 22. Step 6 — Extract VID/PID

Windows exposes hardware identifiers.

Conceptually:

```text
Hardware ID
   ↓
USB\VID_0951&PID_1666...
   ↓
Parse
   ↓
VID = 0951
PID = 1666
```

So your C++ code ends up with:

```cpp
device.vendorId = L"0951";
device.productId = L"1666";
```

---

# 23. Step 7 — Extract serial number

From the device's instance/hardware information, Windows may expose:

```text
USB\VID_0951&PID_1666\ABC123456
```

The instance-specific portion can contain the device's serial/instance identifier.

Your code extracts:

```text
ABC123456
```

If the device doesn't expose a serial number, you must handle:

```text
serialNumber = UNKNOWN
```

**Never assume every USB device has a useful serial number.**

---

# 24. Step 8 — Extract product revision

If available:

```text
Product revision
       ↓
device.productRevision
```

Example:

```text
"0200"
```

---

# 25. Step 9 — Extract device description

Windows device properties can provide:

```text
Device Description
```

Example:

```text
USB Mass Storage Device
```

or:

```text
HID-compliant keyboard
```

Store:

```cpp
device.description = ...
```

---

# 26. Step 10 — Determine device class

Now the classifier examines the device interfaces.

For example:

```text
Interface GUID
       ↓
HID interface
       ↓
DeviceType = HID
```

or:

```text
Volume interface
       ↓
Storage
       ↓
DeviceType = STORAGE
```

or:

```text
Network interface
       ↓
Network
       ↓
DeviceType = NETWORK
```

The paper specifically says the classifier uses descriptor information to identify HID, storage and network categories. 

---

# 27. Step 11 — If STORAGE, extract volume information

If:

```text
DeviceType == STORAGE
```

we do additional work.

For example:

```text
Device
 ↓
Drive letter
 ↓
E:
```

Then query:

```text
E:
 ↓
Volume information
```

to obtain:

```text
Volume name
Volume serial
Filesystem
```

Result:

```text
Drive       = E:
VolumeName  = PROJECT_USB
VolumeSerial= 7A3F-21BC
Filesystem  = exFAT
```

---

# 28. Step 12 — Construct your device object

At this point:

```cpp
USBDevice device;
```

might contain:

```text
DeviceType      = STORAGE
DeviceBus       = USB
VendorID        = 0951
ProductID       = 1666
ProductRev      = 0200
SerialNumber    = ABC123456
DeviceKey       = USB\VID_0951...
Drive           = E:
DeviceDesc      = Kingston DataTraveler
DeviceVolName   = PROJECT_USB
DeviceVolSerial = 7A3F-21BC
DeviceVolFS     = exFAT
```

This is essentially what the paper means by **decoding descriptor information into readable device information**. 

---

# 29. Step 13 — Send it to the allowlist controller

Now:

```text
USBDevice
    ↓
Allowlist Controller
```

The controller compares the relevant fields against the allowlist.

The paper says the allowlist contains detailed device information including VID, serial number and partition/volume information. 

For example:

```text
CONNECTED DEVICE

VID       = 0951
PID       = 1666
Serial    = ABC123
Volume    = 7A3F-21BC
Type      = STORAGE
```

versus:

```text
ALLOWLIST

VID       = 0951
PID       = 1666
Serial    = ABC123
Volume    = 7A3F-21BC
Type      = STORAGE
```

→ **MATCH**

→ ALLOW.

---

# 30. Complete call/processing flow

This is the flow you should keep in mind while coding:

```text
Windows Boot
     │
     ▼
USBIPSService::Start()
     │
     ▼
InitializeDatabase()
     │
     ▼
InitializeServerConnection()
     │
     ▼
RegisterDeviceNotifications()
     │
     ▼
     ┌─────────────────────────────┐
     │ WAIT FOR USB EVENT          │
     └──────────────┬──────────────┘
                    │
              USB CONNECTED
                    │
                    ▼
          DeviceNotificationHandler()
                    │
                    ▼
             IsArrivalEvent?
                    │
                    ▼
          GetDeviceInterfacePath()
                    │
                    ▼
        EnumerateDeviceInformation()
                    │
                    ▼
            GetHardwareIDs()
                    │
                    ├── VID
                    └── PID
                    │
                    ▼
            GetSerialNumber()
                    │
                    ▼
         GetProductRevision()
                    │
                    ▼
         GetDeviceDescription()
                    │
                    ▼
          DetectDeviceClass()
                    │
           ┌────────┼────────┐
           ▼        ▼        ▼
          HID    STORAGE   NETWORK
                    │
                    ▼
          GetVolumeInformation()
                    │
             ┌──────┼──────┐
             ▼      ▼      ▼
          Drive  VolName  VolSerial
                    │
                    ▼
             Build USBDevice
                    │
                    ▼
             Local Allowlist
                    │
             ┌──────┴──────┐
             ▼             ▼
           MATCH        NO MATCH
             │             │
           ALLOW       ASK/BLOCK
             │             │
             └──────┬──────┘
                    ▼
               Log Event
                    │
                    ▼
             Server Sync
```

---

# 31. But there is one important change I recommend

Don't reproduce the paper's `CDevice` literally.

For **our project**, I'd make it cleaner:

```cpp
struct USBDevice
{
    std::wstring deviceId;

    std::wstring vendorId;       // VID
    std::wstring productId;      // PID
    std::wstring productRevision;
    std::wstring serialNumber;

    std::wstring deviceKey;
    std::wstring description;

    DeviceType type;
    std::wstring busType;

    // Storage-specific
    std::wstring driveLetter;
    std::wstring volumeName;
    std::wstring volumeSerial;
    std::wstring filesystem;

    // Metadata
    std::wstring firstSeen;
};
```

And:

```cpp
enum class DeviceType
{
    HID,
    STORAGE,
    NETWORK,
    OTHER
};
```

This is easier to maintain than the paper's mixture of fixed-size `TCHAR` arrays.

---

# 32. The most important distinction

Don't think:

```text
USB plugged
     ↓
One function extracts everything
```

It is actually a **chain of queries**.

Think:

```text
                 USB arrival
                     ↓
          Device interface path
                     ↓
           Device enumeration
                     ↓
       ┌─────────────┼─────────────┐
       ↓             ↓             ↓
    Hardware       Device        Class/
     IDs          Properties     Interface
       ↓             ↓             ↓
    VID/PID       Serial/       HID/Storage/
                  Desc/etc.      Network
                     │
                     ▼
             Storage-specific?
                     │
                    YES
                     ↓
             Volume information
                     │
                     ▼
                USBDevice
                     ↓
               Allowlist
```

That's the architecture I would implement.

And the paper's overall workflow confirms this sequence: notification → descriptor retrieval → classifier → decoded device information → allowlist comparison → decision/logging. 

**For the next coding step, we should implement only this first chain:** `RegisterDeviceNotification → DBT_DEVICEARRIVAL → obtain device interface path → enumerate the Windows device → extract VID/PID/Serial/Description`. Once that works for your keyboard, mouse, flash drive and Wi-Fi adapter, we can add classification and the allowlist.

---

# you asked

message time: 2026-08-28 15:16:26

Let's move with the development. First let's implement the USB monitor. Give code for that and steps to implement and test it

---

# chatgpt response

Yes. Let's implement **Phase 1A: USB Monitor** first.

For this first version, **do not extract VID/PID or classify anything yet**. The only responsibility is:

> **Detect when a USB device is connected or disconnected and identify the device-interface event.**

We'll make it a normal C++ console program first. Once it works reliably, we can convert the same core into a Windows Service.

The paper's architecture starts with registering for Windows device notifications and receiving a notification when a relevant USB device is connected. 

---

# 1. What we're building

Our first program will do this:

```text
USBMonitor.exe
      │
      ▼
RegisterDeviceNotification()
      │
      ▼
Wait...
      │
      │ USB inserted
      ▼
WM_DEVICECHANGE
      │
      ▼
DBT_DEVICEARRIVAL
      │
      ▼
Print "USB DEVICE CONNECTED"
```

And when removed:

```text
USB removed
     ↓
WM_DEVICECHANGE
     ↓
DBT_DEVICEREMOVECOMPLETE
     ↓
Print "USB DEVICE REMOVED"
```

---

# 2. Important Windows concept

There are three things to understand.

### `RegisterDeviceNotification()`

Tells Windows:

> "Notify my application when devices belonging to these device interfaces change."

The paper specifically uses this mechanism during daemon startup. 

### `WM_DEVICECHANGE`

Windows sends this message to your application when device configuration changes.

### `DBT_DEVICEARRIVAL`

Means:

> A device/interface has arrived.

And:

### `DBT_DEVICEREMOVECOMPLETE`

Means:

> A device/interface has been removed.

---

# 3. Create the project

Open **Visual Studio**.

Choose:

```text
Create a new project
        ↓
Console App
        ↓
C++
        ↓
Windows
```

Name it:

```text
USBIPSClient
```

For now, use:

```text
C++17
```

or newer.

Your project:

```text
USBIPSClient/
    USBIPSClient.cpp
```

---

# 4. Replace the code with this

```cpp
#include <windows.h>
#include <dbt.h>
#include <iostream>
#include <string>

// ------------------------------------------------------------
// USB device notification GUIDs
// ------------------------------------------------------------

// Generic USB device interface
DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE,
    0xA5DCBF10, 0x6530, 0x11D2,
    0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

// HID device interface
DEFINE_GUID(GUID_DEVINTERFACE_HID,
    0x4D1E55B2, 0xF16F, 0x11CF,
    0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);

// Storage volume interface
DEFINE_GUID(GUID_DEVINTERFACE_VOLUME,
    0x53F5630D, 0xB6BF, 0x11D0,
    0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B);

// ------------------------------------------------------------
// Global notification handle
// ------------------------------------------------------------

HDEVNOTIFY g_hDeviceNotify = nullptr;


// ------------------------------------------------------------
// Convert device notification type to readable text
// ------------------------------------------------------------

std::wstring GetEventName(DWORD eventType)
{
    switch (eventType)
    {
    case DBT_DEVICEARRIVAL:
        return L"DEVICE ARRIVAL";

    case DBT_DEVICEREMOVECOMPLETE:
        return L"DEVICE REMOVED";

    case DBT_DEVNODES_CHANGED:
        return L"DEVICE NODES CHANGED";

    default:
        return L"OTHER DEVICE EVENT";
    }
}


// ------------------------------------------------------------
// Process device change notification
// ------------------------------------------------------------

void HandleDeviceChange(WPARAM wParam, LPARAM lParam)
{
    if (wParam == DBT_DEVICEARRIVAL)
    {
        std::wcout << L"\n========================================\n";
        std::wcout << L"USB DEVICE CONNECTED\n";
        std::wcout << L"========================================\n";

        PDEV_BROADCAST_HDR header =
            reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);

        if (header == nullptr)
            return;

        if (header->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
        {
            PDEV_BROADCAST_DEVICEINTERFACE_W deviceInterface =
                reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE_W>(lParam);

            std::wcout << L"Device Interface:\n";
            std::wcout << deviceInterface->dbcc_name << L"\n";
        }
    }

    else if (wParam == DBT_DEVICEREMOVECOMPLETE)
    {
        std::wcout << L"\n========================================\n";
        std::wcout << L"USB DEVICE REMOVED\n";
        std::wcout << L"========================================\n";

        PDEV_BROADCAST_HDR header =
            reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);

        if (header == nullptr)
            return;

        if (header->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
        {
            PDEV_BROADCAST_DEVICEINTERFACE_W deviceInterface =
                reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE_W>(lParam);

            std::wcout << L"Device Interface:\n";
            std::wcout << deviceInterface->dbcc_name << L"\n";
        }
    }
}


// ------------------------------------------------------------
// Register for USB device notifications
// ------------------------------------------------------------

bool RegisterUSBNotifications(HWND hWnd)
{
    DEV_BROADCAST_DEVICEINTERFACE_W notificationFilter{};

    notificationFilter.dbcc_size =
        sizeof(DEV_BROADCAST_DEVICEINTERFACE_W);

    notificationFilter.dbcc_devicetype =
        DBT_DEVTYP_DEVICEINTERFACE;

    notificationFilter.dbcc_classguid =
        GUID_DEVINTERFACE_USB_DEVICE;

    g_hDeviceNotify = RegisterDeviceNotificationW(
        hWnd,
        &notificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );

    if (g_hDeviceNotify == nullptr)
    {
        std::wcerr
            << L"Failed to register USB notifications.\n"
            << L"Error code: "
            << GetLastError()
            << L"\n";

        return false;
    }

    return true;
}


// ------------------------------------------------------------
// Window procedure
// ------------------------------------------------------------

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DEVICECHANGE:

        HandleDeviceChange(wParam, lParam);

        return TRUE;


    case WM_DESTROY:

        if (g_hDeviceNotify != nullptr)
        {
            UnregisterDeviceNotification(
                g_hDeviceNotify
            );

            g_hDeviceNotify = nullptr;
        }

        PostQuitMessage(0);

        return 0;
    }

    return DefWindowProcW(
        hwnd,
        uMsg,
        wParam,
        lParam
    );
}


// ------------------------------------------------------------
// Create hidden window
// ------------------------------------------------------------

HWND CreateNotificationWindow(HINSTANCE hInstance)
{
    const wchar_t CLASS_NAME[] =
        L"USBIPSNotificationWindow";

    WNDCLASSW wc{};

    wc.lpfnWndProc =
        WindowProc;

    wc.hInstance =
        hInstance;

    wc.lpszClassName =
        CLASS_NAME;

    if (!RegisterClassW(&wc))
    {
        std::wcerr
            << L"Failed to register window class.\n";

        return nullptr;
    }

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"USBIPS Device Monitor",
        0,
        0,
        0,
        0,
        0,
        HWND_MESSAGE,
        nullptr,
        hInstance,
        nullptr
    );

    if (hwnd == nullptr)
    {
        std::wcerr
            << L"Failed to create notification window.\n";

        return nullptr;
    }

    return hwnd;
}


// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main()
{
    std::wcout
        << L"========================================\n"
        << L"       USBIPS USB DEVICE MONITOR\n"
        << L"========================================\n\n";

    HINSTANCE hInstance =
        GetModuleHandleW(nullptr);

    HWND hwnd =
        CreateNotificationWindow(hInstance);

    if (hwnd == nullptr)
    {
        return 1;
    }

    if (!RegisterUSBNotifications(hwnd))
    {
        DestroyWindow(hwnd);
        return 1;
    }

    std::wcout
        << L"USB monitoring started.\n"
        << L"Waiting for USB devices...\n\n";

    MSG msg{};

    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
```

---

# 5. One thing about the code

You might notice something strange:

We're building a **console application**, but we're creating a hidden Windows window.

That's intentional.

Windows device notifications are delivered through the Windows message mechanism:

```text
WM_DEVICECHANGE
```

So we need something with a message loop.

Our hidden window gives us:

```text
Hidden Window
     ↓
WM_DEVICECHANGE
     ↓
USB Event Handler
```

The console is simply there so we can see the results while developing.

---

# 6. Build it

In Visual Studio:

```text
Build
   ↓
Build Solution
```

You should get:

```text
Build succeeded
```

Run:

```text
Ctrl + F5
```

You should see:

```text
========================================
       USBIPS USB DEVICE MONITOR
========================================

USB monitoring started.
Waiting for USB devices...
```

Leave the program running.

---

# 7. Test 1 — USB flash drive

Now connect a USB flash drive.

You should see something similar to:

```text
========================================
USB DEVICE CONNECTED
========================================
Device Interface:
\\?\USB#VID_0951&PID_1666#...
```

The exact value will depend on your device.

**Do not worry about understanding the entire path yet.**

This is actually extremely important.

We've now reached:

```text
Physical USB
      ↓
Windows Plug & Play
      ↓
Windows device notification
      ↓
WM_DEVICECHANGE
      ↓
Our C++ program
      ↓
Device interface path
```

That interface path will become the input to our **next module: Device Identification**.

---

# 8. Test 2 — Remove the USB

Remove the flash drive.

You should get:

```text
========================================
USB DEVICE REMOVED
========================================
Device Interface:
\\?\USB#VID_0951&PID_1666#...
```

So you've verified both directions:

```text
INSERT
  ↓
DBT_DEVICEARRIVAL

REMOVE
  ↓
DBT_DEVICEREMOVECOMPLETE
```

---

# 9. Test with your keyboard

Now leave the program running and connect a USB keyboard.

You should receive another:

```text
USB DEVICE CONNECTED
```

with a device-interface path.

Do the same with:

- USB mouse
- USB flash drive
- USB Wi-Fi adapter

if you have them.

At this stage **we are not classifying them yet**.

The objective is simply:

> **USBIPS reliably receives device arrival/removal events.**

---

# 10. What exactly happens internally?

Let's follow one USB flash drive from physical insertion.

### Stage 1

You physically insert:

```text
USB Flash Drive
```

↓

### Stage 2

USB electrical/USB protocol communication occurs.

↓

### Stage 3

Windows USB stack detects the device.

↓

### Stage 4

Windows Plug and Play manager creates/updates the corresponding device/interface information.

↓

### Stage 5

Our program has previously called:

```cpp
RegisterDeviceNotificationW(...)
```

↓

### Stage 6

Windows sends:

```cpp
WM_DEVICECHANGE
```

↓

### Stage 7

Our:

```cpp
WindowProc()
```

receives it.

↓

### Stage 8

We call:

```cpp
HandleDeviceChange(wParam, lParam);
```

↓

### Stage 9

We check:

```cpp
wParam == DBT_DEVICEARRIVAL
```

↓

### Stage 10

We interpret:

```cpp
lParam
```

as:

```cpp
PDEV_BROADCAST_HDR
```

↓

### Stage 11

We check:

```cpp
header->dbch_devicetype
```

↓

### Stage 12

For a device-interface event:

```cpp
DBT_DEVTYP_DEVICEINTERFACE
```

↓

### Stage 13

We interpret it as:

```cpp
PDEV_BROADCAST_DEVICEINTERFACE_W
```

↓

### Stage 14

Finally:

```cpp
deviceInterface->dbcc_name
```

gives us the device interface path.

For example:

```text
\\?\USB#VID_0951&PID_1666#...
```

---

# 11. Why this interface path is important

This is the bridge between **detection** and **identification**.

Right now:

```text
dbcc_name
    ↓
\\?\USB#VID_0951&PID_1666#...
```

Later:

```text
dbcc_name
    ↓
Windows device APIs
    ↓
VID
PID
Serial
Description
Class
Device Key
...
```

So the next stage will look like:

```text
              USB INSERT
                   ↓
          RegisterDeviceNotification
                   ↓
            WM_DEVICECHANGE
                   ↓
            DBT_DEVICEARRIVAL
                   ↓
             dbcc_name
                   ↓
       ┌──────────────────────┐
       │ DEVICE IDENTIFIER    │
       │      MODULE          │
       └──────────────────────┘
                   ↓
                VID
                PID
              Serial
            Description
              DeviceKey
                   ↓
             USBDevice
```

---

# 12. One correction to the paper's implementation for our project

The paper describes the daemon receiving notifications from its kernel modules before the classifier gets descriptor information. 

For **our first implementation**, I recommend not building a custom kernel module.

We'll use:

```text
Windows PnP
    ↓
Windows API
    ↓
User-mode USBIPS Service
```

That is substantially simpler and appropriate for Phase 1.

Later, if we discover that Windows user-mode access control isn't sufficient for the actual blocking requirement, **then** we can evaluate whether a kernel component is necessary.

---

# 13. What you should NOT implement yet

Don't add these now:

```text
❌ VID/PID extraction
❌ Serial extraction
❌ Device classification
❌ SQLite
❌ Server
❌ Allowlist
❌ Blocking
❌ HID detection
❌ Storage monitoring
❌ Network monitoring
```

We're deliberately keeping this module small.

The first milestone is:

> **"USBIPS can reliably detect USB device arrival and removal."**

---

# 14. Your first test checklist

Run the program and test:

| Test | Action | Expected |
|---|---|---|
| 1 | Start USBIPS | Monitoring starts |
| 2 | Insert flash drive | `DEVICE CONNECTED` |
| 3 | Remove flash drive | `DEVICE REMOVED` |
| 4 | Insert keyboard | `DEVICE CONNECTED` |
| 5 | Remove keyboard | `DEVICE REMOVED` |
| 6 | Insert mouse | `DEVICE CONNECTED` |
| 7 | Remove mouse | `DEVICE REMOVED` |
| 8 | Insert another USB | `DEVICE CONNECTED` |

If all of these work, **Phase 1A is complete**.

---

## Then we move to Phase 1B

Once you confirm this program works, the next module is much more interesting:

```text
dbcc_name
    ↓
Windows Device Information APIs
    ↓
VID
PID
Serial Number
Device Description
Hardware IDs
Device Class
Device Key
    ↓
USBDevice structure
```

And I'll give you the **actual C++ implementation using Windows SetupAPI/Configuration Manager**, including which API call extracts **each individual field**, what its input/output is, and how we handle devices that don't expose a serial number.

---

# you asked

message time: 2026-08-28 16:02:06

Create a new project
        ↓
Console App
        ↓
C++
        ↓
Windows

What does this mean ? Can I create a normal folder or it requires some environment to be set up

---

# chatgpt response

It requires a **development environment**, but you don't need anything complicated. The phrase I gave you was specifically describing how to create a **Visual Studio C++ project**, not just a normal folder.

### What I mean by "Create a new project"

For this project, I recommend using **Visual Studio** on Windows.

Think of it like this:

```text
Normal folder
    ↓
just stores files

Visual Studio Project
    ↓
stores files + knows how to compile C++
    ↓
produces USBIPSClient.exe
```

So **don't just create a folder and paste the code into a `.cpp` file yet**.

---

# 1. What you need to install

You need:

### Visual Studio Community

You can use the free **Visual Studio Community** edition.

During installation, select the workload:

> **Desktop development with C++**

This installs the important components:

```text
Visual Studio
├── C++ compiler
├── Windows SDK
├── MSVC
├── Debugger
└── Build tools
```

You don't need to install a separate C++ compiler.

---

# 2. Then create the project

After installing Visual Studio:

Open:

```text
Visual Studio
```

You'll see something like:

```text
┌─────────────────────────────────────┐
│ Visual Studio                       │
│                                     │
│  Create a new project               │
│                                     │
│  Open a project or solution         │
│                                     │
└─────────────────────────────────────┘
```

Choose:

> **Create a new project**

---

# 3. Select the C++ Console App

You'll get a list of project templates.

Search:

```text
Console App
```

Select the **C++ Console App** template.

Don't worry if you see multiple console-related templates. We want the one using **C++**.

Then click:

> **Next**

---

# 4. Give it a name

Use:

```text
USBIPSClient
```

For example:

```text
Location:
D:\Projects\USBIPS
```

Visual Studio will create something like:

```text
D:\Projects\USBIPS\
│
├── USBIPSClient\
│   ├── USBIPSClient.cpp
│   ├── USBIPSClient.vcxproj
│   └── ...
```

You don't need to manually create these files.

---

# 5. What does "Windows" mean in my previous message?

This part:

```text
Create a new project
        ↓
Console App
        ↓
C++
        ↓
Windows
```

wasn't meant to mean that you need to select a special **"Windows" option** after C++.

I was simply saying:

> We're developing a **Windows C++ application**.

The actual process is closer to:

```text
Visual Studio
     ↓
Create New Project
     ↓
C++ Console App
     ↓
USBIPSClient
     ↓
Windows C++ application
```

The code uses Windows APIs such as:

```cpp
#include <windows.h>
#include <dbt.h>
```

which is why we're developing this specifically for Windows.

---

# 6. Do you need to install anything else right now?

For **Phase 1A**, no.

You need only:

### Required

- Windows
- Visual Studio Community
- Desktop development with C++
- Windows SDK

### Not needed yet

- Python
- FastAPI
- SQLite
- Git
- WDK
- Raspberry Pi
- Visual Studio Code
- CMake

We'll introduce those when their respective modules require them.

---

# 7. Verify your installation before writing USBIPS code

Once the project is created, Visual Studio will generate a simple C++ program.

It will look approximately like:

```cpp
#include <iostream>

int main()
{
    std::cout << "Hello World!\n";
}
```

Press:

**Ctrl + F5**

You should get:

```text
Hello World!
```

If that works, your C++ development environment is ready.

Then replace that code with the **USB monitor code** I gave you.

---

## So, in short

You **can create a normal folder**, but the folder by itself isn't enough to compile and debug this C++ Windows application conveniently.

Use:

```text
Install Visual Studio
        ↓
Desktop development with C++
        ↓
Create C++ Console App
        ↓
Name: USBIPSClient
        ↓
Test "Hello World"
        ↓
Add USB monitor code
        ↓
Build
        ↓
USBIPSClient.exe
```

**Don't install the Windows Driver Kit (WDK) yet.** We're deliberately starting in user mode, and the first USB-monitor module only needs the normal Windows SDK/API environment.