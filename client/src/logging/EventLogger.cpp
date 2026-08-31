#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "EventLogger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <random>

EventLogger::EventLogger() {}

EventLogger::~EventLogger() {
    Close();
}

bool EventLogger::Initialize(LocalDatabase* db, const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_db = db;
    m_clientId = LoadOrCreateClientId();

    m_logFile.open(logFilePath, std::ios::app);
    if (!m_logFile.is_open()) {
        std::cerr << "[EventLogger] Failed to open log file: " << logFilePath << "\n";
        return false;
    }

    return true;
}

void EventLogger::Close() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

// ── Timestamp & ID Generators ─────────────────────────────────

std::string EventLogger::GetIso8601Timestamp() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    std::ostringstream oss;
    oss << std::setfill('0')
        << st.wYear << '-'
        << std::setw(2) << st.wMonth  << '-'
        << std::setw(2) << st.wDay    << 'T'
        << std::setw(2) << st.wHour   << ':'
        << std::setw(2) << st.wMinute << ':'
        << std::setw(2) << st.wSecond << 'Z';
    return oss.str();
}

std::string EventLogger::GenerateEventId() {
    SYSTEMTIME st;
    GetSystemTime(&st);

    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    uint32_t randPart = dist(rng);

    std::ostringstream oss;
    oss << std::setfill('0')
        << st.wYear
        << std::setw(2) << st.wMonth
        << std::setw(2) << st.wDay  << 'T'
        << std::setw(2) << st.wHour
        << std::setw(2) << st.wMinute
        << std::setw(2) << st.wSecond << 'Z'
        << '-'
        << std::hex << std::setw(4) << (randPart & 0xFFFF);
    return oss.str();
}

std::string EventLogger::GenerateClientId() {
    char hostname[256] = {};
    DWORD size = sizeof(hostname);
    GetComputerNameA(hostname, &size);

    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    uint32_t randPart = dist(rng);

    std::ostringstream oss;
    oss << "USBIPS-" << hostname << '-'
        << std::hex << std::setw(8) << std::setfill('0') << randPart;
    return oss.str();
}

std::string EventLogger::LoadOrCreateClientId(const std::string& confPath) {
    // Try to read existing client ID from config file
    std::ifstream confIn(confPath);
    if (confIn.is_open()) {
        std::string line;
        while (std::getline(confIn, line)) {
            const std::string prefix = "client_id=";
            if (line.substr(0, prefix.size()) == prefix) {
                std::string id = line.substr(prefix.size());
                if (!id.empty()) return id;
            }
        }
    }

    // Generate a new client ID and persist it
    std::string newId = GenerateClientId();
    std::ofstream confOut(confPath);
    if (confOut.is_open()) {
        confOut << "client_id=" << newId << "\n";
    }

    return newId;
}

// ── Core Logging ──────────────────────────────────────────────

SecurityEvent EventLogger::BuildEvent(EventType type, const USBDevice& device,
                                      const std::string& decision, const std::string& reason) {
    SecurityEvent ev;
    ev.eventId    = GenerateEventId();
    ev.eventType  = EventTypeToString(type);
    ev.clientId   = m_clientId;
    ev.timestamp  = GetIso8601Timestamp();
    ev.device     = device;
    ev.decision   = decision;
    ev.reason     = reason;
    ev.syncedToServer = false;
    return ev;
}

void EventLogger::Log(SecurityEvent& event) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (event.eventId.empty()) event.eventId = GenerateEventId();
    if (event.timestamp.empty()) event.timestamp = GetIso8601Timestamp();
    if (event.clientId.empty()) event.clientId = m_clientId;

    // Pipe-separated log line
    // Format: <timestamp> | <eventType> | <deviceType> | VID:<vid> PID:<pid> SN:<sn> | <decision> | <reason>
    std::string vidStr   = LocalDatabase::ToUtf8(event.device.vid);
    std::string pidStr   = LocalDatabase::ToUtf8(event.device.pid);
    std::string snStr    = LocalDatabase::ToUtf8(event.device.serialNumber);
    std::string typeStr  = LocalDatabase::ToUtf8(event.device.deviceType);
    if (typeStr.empty()) typeStr = "UNKNOWN";

    std::ostringstream line;
    line << event.timestamp
         << " | " << event.eventType
         << " | " << typeStr
         << " | VID:" << (vidStr.empty() ? "---" : vidStr)
         << " PID:" << (pidStr.empty() ? "---" : pidStr)
         << (snStr.empty() ? "" : (" SN:" + snStr))
         << " | " << (event.decision.empty() ? "-" : event.decision)
         << " | " << (event.reason.empty() ? "-" : event.reason)
         << " | ClientID:" << event.clientId
         << " | EventID:" << event.eventId;

    // 1. Write to log file
    if (m_logFile.is_open()) {
        m_logFile << line.str() << "\n";
        m_logFile.flush();
    }

    // 2. Print to console
    std::cout << "[LOG] " << line.str() << "\n";

    // 3. Write to SQLite events table
    if (m_db) {
        m_db->LogEvent(event.eventType, event.device, event.decision, event.reason);
    }
}

// ── Sync Helpers ──────────────────────────────────────────────

std::vector<DeviceEventRecord> EventLogger::GetUnsynced() {
    if (!m_db) return {};
    return m_db->GetPendingEventRecords();
}

void EventLogger::MarkSynced(const std::vector<int>& ids) {
    if (!m_db) return;
    m_db->MarkEventsSynced(ids);
}
