#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <string>
#include <vector>

#include "../device/USBDevice.h"

// libcurl is the HTTP client used by the Windows C++ client in Task 9.
// nlohmann/json is used to build and parse JSON payloads.
// These dependencies are expected to be provided by the Windows build environment.
//
// Build note:
//   - install libcurl for Windows (e.g. vcpkg / Visual Studio package manager)
//   - include the nlohmann/json header-only package
//   - link against libcurl

class ServerClient {
public:
    struct PolicyResult {
        bool success = false;
        bool allowed = false;
        std::string policy = "BLOCKED";
        std::string reason = "SERVER_UNAVAILABLE";
    };

    explicit ServerClient(
        const std::string& serverBaseUrl = "http://127.0.0.1:8000",
        const std::string& clientId = ""
    );

    void SetBaseUrl(const std::string& serverBaseUrl);
    void SetClientId(const std::string& clientId);
    const std::string& GetClientId() const { return m_clientId; }
    const std::string& GetBaseUrl() const { return m_baseUrl; }

    // Device policy check against the server; expected to receive values already
    // produced by the existing C++ monitor/extractor pipeline.
    PolicyResult CheckDevicePolicy(const std::wstring& vid,
                                   const std::wstring& pid,
                                   const std::wstring& serial = L"",
                                   const std::wstring& deviceType = L"");

    // Register the local client with the server.
    bool RegisterClient(const std::string& hostname,
                        const std::string& ipAddress,
                        const std::string& os,
                        const std::string& version);

    // Send a heartbeat to the server.
    bool SendHeartbeat(const std::string& status = "ONLINE");

    // Synchronize a single event JSON payload or explicitly built event record.
    bool SendEvent(const std::string& eventJson);

    // Fetch the centralized allowlist from /api/devices.
    std::vector<USBDevice> FetchAllowlist();

private:
    std::string m_baseUrl;
    std::string m_clientId;

    std::string HttpPostJson(const std::string& endpoint, const std::string& jsonBody);
    std::string HttpGet(const std::string& endpoint);
    static std::string WStringToUtf8(const std::wstring& value);
    static std::wstring Utf8ToWString(const std::string& value);
};
