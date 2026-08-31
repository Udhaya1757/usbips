// ============================================================
// LocalDatabase.cpp
// USBIPS — USB Intrusion Prevention System
// Phase 1D: Local SQLite Allowlist & Event Storage Implementation
// ============================================================

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

#include "LocalDatabase.h"
#include <iostream>
#include <sstream>

// ── UTF-8 / UTF-16 Conversion Helpers ────────────────────────

std::string LocalDatabase::ToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0) return "";
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), &result[0], sizeNeeded, nullptr, nullptr);
    return result;
}

std::wstring LocalDatabase::ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (sizeNeeded <= 0) return L"";
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &result[0], sizeNeeded);
    return result;
}

// ── Lifecycle ────────────────────────────────────────────────

LocalDatabase::LocalDatabase()
    : m_db(nullptr)
{
}

LocalDatabase::~LocalDatabase() {
    Close();
}

bool LocalDatabase::Initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_db) {
        Close();
    }

    m_dbPath = dbPath;
    int rc = sqlite3_open_v2(
        m_dbPath.c_str(),
        &m_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr
    );

    if (rc != SQLITE_OK) {
        std::cerr << "[LocalDatabase] Failed to open SQLite DB: "
                  << (m_db ? sqlite3_errmsg(m_db) : "Unknown error") << std::endl;
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }

    // Enable WAL (Write-Ahead Logging) mode for enhanced concurrency and durability
    char* zErrMsg = nullptr;
    sqlite3_exec(m_db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, &zErrMsg);
    if (zErrMsg) {
        sqlite3_free(zErrMsg);
    }

    return CreateTables();
}

void LocalDatabase::Close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool LocalDatabase::CreateTables() {
    const char* schema =
        "CREATE TABLE IF NOT EXISTS allowlist ("
        "    id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    vid             TEXT NOT NULL,"
        "    pid             TEXT NOT NULL,"
        "    serial_number   TEXT,"
        "    device_type     TEXT NOT NULL,"
        "    description     TEXT,"
        "    volume_serial   TEXT,"
        "    status          TEXT DEFAULT 'ALLOWED',"
        "    friendly_name   TEXT,"
        "    created_at      DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    updated_at      DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE TABLE IF NOT EXISTS events ("
        "    id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    event_type      TEXT NOT NULL,"
        "    vid             TEXT,"
        "    pid             TEXT,"
        "    serial_number   TEXT,"
        "    device_type     TEXT,"
        "    decision        TEXT,"
        "    reason          TEXT,"
        "    timestamp       DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    synced_to_server INTEGER DEFAULT 0"
        ");";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, schema, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[LocalDatabase] Table creation error: "
                  << (errMsg ? errMsg : sqlite3_errmsg(m_db)) << std::endl;
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }

    return true;
}

// ── Allowlist Matching ───────────────────────────────────────
// Matching Rule:
//   A device is ALLOWED if:
//     1. status == 'ALLOWED'
//     2. UPPER(vid) == UPPER(device.vid)
//     3. UPPER(pid) == UPPER(device.pid)
//     4. (serial_number IS NULL OR serial_number == '' OR UPPER(serial_number) == UPPER(device.serialNumber))
bool LocalDatabase::IsDeviceAllowed(const USBDevice& device) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    const char* sql =
        "SELECT id FROM allowlist "
        "WHERE status = 'ALLOWED' "
        "  AND UPPER(vid) = UPPER(?) "
        "  AND UPPER(pid) = UPPER(?) "
        "  AND (serial_number IS NULL OR serial_number = '' OR UPPER(serial_number) = UPPER(?)) "
        "LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[LocalDatabase] Query prepare error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    std::string vidStr = ToUtf8(device.vid);
    std::string pidStr = ToUtf8(device.pid);
    std::string serialStr = ToUtf8(device.serialNumber);

    sqlite3_bind_text(stmt, 1, vidStr.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pidStr.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, serialStr.c_str(), -1, SQLITE_STATIC);

    bool allowed = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        allowed = true;
    }

    sqlite3_finalize(stmt);
    return allowed;
}

bool LocalDatabase::AddDevice(const USBDevice& device, const std::string& friendlyName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    const char* sql =
        "INSERT INTO allowlist (vid, pid, serial_number, device_type, description, volume_serial, status, friendly_name) "
        "VALUES (?, ?, ?, ?, ?, ?, 'ALLOWED', ?);";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    std::string vid = ToUtf8(device.vid);
    std::string pid = ToUtf8(device.pid);
    std::string serial = ToUtf8(device.serialNumber);
    std::string type = ToUtf8(device.deviceType.empty() ? L"OTHER" : device.deviceType);
    std::string desc = ToUtf8(device.description);
    std::string volSer = ToUtf8(device.volumeSerial);
    std::string name = friendlyName.empty() ? desc : friendlyName;

    sqlite3_bind_text(stmt, 1, vid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, serial.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, desc.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, volSer.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, name.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

bool LocalDatabase::RemoveDevice(const std::string& vid, const std::string& pid, const std::string& serial) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    std::string sql;
    if (serial.empty()) {
        sql = "DELETE FROM allowlist WHERE UPPER(vid) = UPPER(?) AND UPPER(pid) = UPPER(?);";
    } else {
        sql = "DELETE FROM allowlist WHERE UPPER(vid) = UPPER(?) AND UPPER(pid) = UPPER(?) AND UPPER(serial_number) = UPPER(?);";
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, vid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pid.c_str(), -1, SQLITE_STATIC);
    if (!serial.empty()) {
        sqlite3_bind_text(stmt, 3, serial.c_str(), -1, SQLITE_STATIC);
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

bool LocalDatabase::UpdateDeviceList(const std::vector<USBDevice>& serverList) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    char* errMsg = nullptr;
    sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free(errMsg);

    // Clear existing allowlist entries
    sqlite3_exec(m_db, "DELETE FROM allowlist WHERE status = 'ALLOWED';", nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free(errMsg);

    const char* sql =
        "INSERT INTO allowlist (vid, pid, serial_number, device_type, description, volume_serial, status, friendly_name) "
        "VALUES (?, ?, ?, ?, ?, ?, 'ALLOWED', ?);";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& dev : serverList) {
        sqlite3_reset(stmt);

        std::string vid = ToUtf8(dev.vid);
        std::string pid = ToUtf8(dev.pid);
        std::string serial = ToUtf8(dev.serialNumber);
        std::string type = ToUtf8(dev.deviceType.empty() ? L"OTHER" : dev.deviceType);
        std::string desc = ToUtf8(dev.description);
        std::string volSer = ToUtf8(dev.volumeSerial);
        std::string name = desc;

        sqlite3_bind_text(stmt, 1, vid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, pid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, serial.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, type.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, desc.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, volSer.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, name.c_str(), -1, SQLITE_STATIC);

        sqlite3_step(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

// ── Event Logging & Synchronization ──────────────────────────

bool LocalDatabase::LogEvent(const std::string& eventType, const USBDevice& device,
                            const std::string& decision, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    const char* sql =
        "INSERT INTO events (event_type, vid, pid, serial_number, device_type, decision, reason, synced_to_server) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, 0);";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    std::string vid = ToUtf8(device.vid);
    std::string pid = ToUtf8(device.pid);
    std::string serial = ToUtf8(device.serialNumber);
    std::string type = ToUtf8(device.deviceType);

    sqlite3_bind_text(stmt, 1, eventType.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, vid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, pid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, serial.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, decision.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, reason.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

std::vector<DeviceEventRecord> LocalDatabase::GetPendingEventRecords() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DeviceEventRecord> records;
    if (!m_db) return records;

    const char* sql =
        "SELECT id, event_type, vid, pid, serial_number, device_type, decision, reason, timestamp, synced_to_server "
        "FROM events WHERE synced_to_server = 0 ORDER BY id ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return records;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DeviceEventRecord rec;
        rec.id = sqlite3_column_int(stmt, 0);
        rec.eventType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.vid = sqlite3_column_text(stmt, 2) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)) : "";
        rec.pid = sqlite3_column_text(stmt, 3) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)) : "";
        rec.serialNumber = sqlite3_column_text(stmt, 4) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)) : "";
        rec.deviceType = sqlite3_column_text(stmt, 5) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)) : "";
        rec.decision = sqlite3_column_text(stmt, 6) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)) : "";
        rec.reason = sqlite3_column_text(stmt, 7) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7)) : "";
        rec.timestamp = sqlite3_column_text(stmt, 8) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)) : "";
        rec.syncedToServer = sqlite3_column_int(stmt, 9);
        records.push_back(rec);
    }

    sqlite3_finalize(stmt);
    return records;
}

std::vector<USBDevice> LocalDatabase::GetPendingEvents() {
    auto records = GetPendingEventRecords();
    std::vector<USBDevice> devices;
    for (const auto& rec : records) {
        USBDevice dev;
        dev.vid = ToWide(rec.vid);
        dev.pid = ToWide(rec.pid);
        dev.serialNumber = ToWide(rec.serialNumber);
        dev.deviceType = ToWide(rec.deviceType);
        dev.description = ToWide(rec.reason);
        devices.push_back(dev);
    }
    return devices;
}

bool LocalDatabase::MarkEventsSynced(const std::vector<int>& eventIds) {
    if (eventIds.empty()) return true;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    std::stringstream ss;
    ss << "UPDATE events SET synced_to_server = 1 WHERE id IN (";
    for (size_t i = 0; i < eventIds.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << eventIds[i];
    }
    ss << ");";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, ss.str().c_str(), nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free(errMsg);

    return (rc == SQLITE_OK);
}
