#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "../device/USBDevice.h"
#include "../database/LocalDatabase.h"
#include <string>
#include <vector>
#include <fstream>
#include <mutex>

// ── Event Type Enum ──────────────────────────────────────────
enum class EventType {
    DEVICE_CONNECTED,
    DEVICE_REMOVED,
    ALLOWLIST_MATCH,
    UNKNOWN_DEVICE,
    USER_APPROVED,
    USER_REJECTED,
    DEVICE_BLOCKED,
    SERVER_SYNC_SUCCESS,
    SERVER_SYNC_FAILED
};

inline std::string EventTypeToString(EventType type) {
    switch (type) {
        case EventType::DEVICE_CONNECTED:    return "DEVICE_CONNECTED";
        case EventType::DEVICE_REMOVED:      return "DEVICE_REMOVED";
        case EventType::ALLOWLIST_MATCH:     return "ALLOWLIST_MATCH";
        case EventType::UNKNOWN_DEVICE:      return "UNKNOWN_DEVICE";
        case EventType::USER_APPROVED:       return "USER_APPROVED";
        case EventType::USER_REJECTED:       return "USER_REJECTED";
        case EventType::DEVICE_BLOCKED:      return "DEVICE_BLOCKED";
        case EventType::SERVER_SYNC_SUCCESS: return "SERVER_SYNC_SUCCESS";
        case EventType::SERVER_SYNC_FAILED:  return "SERVER_SYNC_FAILED";
        default:                             return "UNKNOWN";
    }
}

// ── SecurityEvent Struct ─────────────────────────────────────
struct SecurityEvent {
    std::string eventId;         // timestamp-based unique ID e.g. "20260831T115900Z-a3f7"
    std::string eventType;       // EventType as string
    std::string clientId;        // USBIPS-<hostname>-<8hexchars>
    std::string timestamp;       // ISO 8601 e.g. "2026-08-31T11:59:00Z"
    USBDevice   device;          // the device that triggered this event
    std::string decision;        // "ALLOW", "BLOCK", "ASK"
    std::string reason;          // "ALLOWLIST_MATCH", "USER_APPROVED", etc.
    bool        syncedToServer = false;
};

// ── EventLogger Class ────────────────────────────────────────
class EventLogger {
public:
    EventLogger();
    ~EventLogger();

    EventLogger(const EventLogger&) = delete;
    EventLogger& operator=(const EventLogger&) = delete;

    bool Initialize(LocalDatabase* db, const std::string& logFilePath = "usbips_events.log");
    void Close();

    // Write a SecurityEvent to the log file and the database
    void Log(SecurityEvent& event);

    // Convenience builders
    SecurityEvent BuildEvent(EventType type, const USBDevice& device,
                             const std::string& decision, const std::string& reason);

    // Returns unsynced events from the database
    std::vector<DeviceEventRecord> GetUnsynced();

    // Mark a specific eventId as synced
    void MarkSynced(const std::vector<int>& ids);

    const std::string& GetClientId() const { return m_clientId; }

private:
    std::string GenerateEventId();
    std::string GetIso8601Timestamp();
    std::string LoadOrCreateClientId(const std::string& confPath = "usbips_client.conf");
    std::string GenerateClientId();

    LocalDatabase* m_db = nullptr;
    std::ofstream  m_logFile;
    std::string    m_clientId;
    std::mutex     m_mutex;
};
