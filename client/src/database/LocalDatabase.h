#pragma once

// ============================================================
// LocalDatabase.h
// USBIPS — USB Intrusion Prevention System
// Phase 1D: Local SQLite Allowlist & Event Storage
// ============================================================
// Purpose:
// Manages the local SQLite database (usbips_local.db) for:
//   1. Trusted device Allowlist matching (offline-capable).
//   2. Local audit event logging (arrival, removal, block, allow).
//   3. Pending event buffering for server synchronization.
// ============================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "../device/USBDevice.h"
#include "sqlite3.h"

#include <string>
#include <vector>
#include <mutex>

// Structure representing an audit event in the local database
struct DeviceEventRecord {
    int id = 0;
    std::string eventType;     // "CONNECTED", "DISCONNECTED", "EVALUATED"
    std::string vid;
    std::string pid;
    std::string serialNumber;
    std::string deviceType;
    std::string decision;      // "ALLOW", "BLOCK", "ASK"
    std::string reason;
    std::string timestamp;
    int syncedToServer = 0;
};

class LocalDatabase {
public:
    LocalDatabase();
    ~LocalDatabase();

    // Disable copy semantics for RAII safety
    LocalDatabase(const LocalDatabase&) = delete;
    LocalDatabase& operator=(const LocalDatabase&) = delete;

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------
    bool Initialize(const std::string& dbPath = "usbips_local.db");
    void Close();
    bool IsOpen() const { return m_db != nullptr; }

    // ------------------------------------------------------------------
    // Allowlist Management
    // ------------------------------------------------------------------
    // Returns true if device matches an entry where status == 'ALLOWED'
    // Matching rule: VID + PID match AND (serial is empty in allowlist OR serial matches)
    bool IsDeviceAllowed(const USBDevice& device);

    // Adds a device to the local allowlist
    bool AddDevice(const USBDevice& device, const std::string& friendlyName = "");

    // Removes a device from the local allowlist by VID/PID and optional serial
    bool RemoveDevice(const std::string& vid, const std::string& pid, const std::string& serial = "");

    // Refreshes the local allowlist from a list of devices (e.g. received from Server)
    bool UpdateDeviceList(const std::vector<USBDevice>& serverList);

    // ------------------------------------------------------------------
    // Event Logging & Synchronization
    // ------------------------------------------------------------------
    // Records an audit event to the 'events' table
    bool LogEvent(const std::string& eventType, const USBDevice& device,
                  const std::string& decision, const std::string& reason);

    // Retrieves all events where synced_to_server == 0
    std::vector<DeviceEventRecord> GetPendingEventRecords();

    // Convenience method returning USBDevice representations of pending events
    std::vector<USBDevice> GetPendingEvents();

    // Marks a list of event IDs as synced_to_server = 1
    bool MarkEventsSynced(const std::vector<int>& eventIds);

    // ------------------------------------------------------------------
    // String Conversion Helpers (UTF-16 <-> UTF-8)
    // ------------------------------------------------------------------
    static std::string ToUtf8(const std::wstring& wstr);
    static std::wstring ToWide(const std::string& str);

private:
    bool CreateTables();

    sqlite3* m_db = nullptr;
    std::string m_dbPath;
    std::mutex m_mutex;
};
