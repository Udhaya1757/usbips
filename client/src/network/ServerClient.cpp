#include "ServerClient.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp);
    if (!buffer) {
        return 0;
    }
    buffer->append(static_cast<char*>(contents), total);
    return total;
}

std::string SafeTrim(const std::string& value) {
    const std::string whitespace = " \t\r\n";
    size_t start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

std::string GetLocalHostname() {
#ifdef _WIN32
    char hostname[256] = {0};
    DWORD size = sizeof(hostname);
    if (GetComputerNameA(hostname, &size)) {
        return std::string(hostname);
    }
#endif
    return "USBIPS-CLIENT";
}

std::string GetLocalOsName() {
#ifdef _WIN32
    return "Windows";
#else
    return "Linux";
#endif
}

std::string GetLocalIpAddress() {
    return "127.0.0.1";
}

} // namespace

ServerClient::ServerClient(const std::string& serverBaseUrl, const std::string& clientId)
    : m_baseUrl(serverBaseUrl), m_clientId(clientId) {
    if (!m_baseUrl.empty() && m_baseUrl.back() == '/') {
        m_baseUrl.pop_back();
    }
}

void ServerClient::SetBaseUrl(const std::string& serverBaseUrl) {
    m_baseUrl = serverBaseUrl;
    if (!m_baseUrl.empty() && m_baseUrl.back() == '/') {
        m_baseUrl.pop_back();
    }
}

void ServerClient::SetClientId(const std::string& clientId) {
    m_clientId = clientId;
}

std::string ServerClient::WStringToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }

    int needed = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.length()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return "";
    }

    std::string converted(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.length()), &converted[0], needed, nullptr, nullptr);
    return converted;
}

std::wstring ServerClient::Utf8ToWString(const std::string& value) {
    if (value.empty()) {
        return L"";
    }

    int needed = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.length()), nullptr, 0);
    if (needed <= 0) {
        return L"";
    }

    std::wstring converted(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.length()), &converted[0], needed);
    return converted;
}

std::string ServerClient::HttpPostJson(const std::string& endpoint, const std::string& jsonBody) {
    if (m_baseUrl.empty()) {
        return "";
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return "";
    }

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string fullUrl = m_baseUrl + endpoint;

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode < 200 || httpCode >= 300) {
        return "";
    }

    return response;
}

std::string ServerClient::HttpGet(const std::string& endpoint) {
    if (m_baseUrl.empty()) {
        return "";
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return "";
    }

    std::string response;
    std::string fullUrl = m_baseUrl + endpoint;

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode < 200 || httpCode >= 300) {
        return "";
    }

    return response;
}

ServerClient::PolicyResult ServerClient::CheckDevicePolicy(const std::wstring& vid,
                                                          const std::wstring& pid,
                                                          const std::wstring& serial,
                                                          const std::wstring& deviceType) {
    PolicyResult result;

    const std::string vidStr = WStringToUtf8(vid);
    const std::string pidStr = WStringToUtf8(pid);
    const std::string serialStr = WStringToUtf8(serial);
    const std::string deviceTypeStr = WStringToUtf8(deviceType);

    nlohmann::json payload = {
        {"vid", vidStr},
        {"pid", pidStr},
        {"serial", serialStr.empty() ? "" : serialStr},
        {"device_type", deviceTypeStr.empty() ? "OTHER" : deviceTypeStr}
    };

    std::string responseJson = HttpPostJson("/api/devices/check", payload.dump());
    if (responseJson.empty()) {
        return result;
    }

    try {
        nlohmann::json parsed = nlohmann::json::parse(responseJson);
        if (parsed.contains("allowed") && parsed["allowed"].is_boolean()) {
            result.allowed = parsed["allowed"].get<bool>();
        }
        if (parsed.contains("policy") && parsed["policy"].is_string()) {
            result.policy = parsed["policy"].get<std::string>();
        }
        if (parsed.contains("reason") && parsed["reason"].is_string()) {
            result.reason = parsed["reason"].get<std::string>();
        }
        result.success = true;
        if (result.policy.empty()) {
            result.policy = result.allowed ? "ALLOWED" : "BLOCKED";
        }
        if (result.reason.empty()) {
            result.reason = result.allowed ? "ALLOWLIST_MATCH" : "BLOCKED";
        }
        return result;
    } catch (const std::exception&) {
        result.reason = "INVALID_SERVER_RESPONSE";
        result.policy = "BLOCKED";
        return result;
    }
}

bool ServerClient::RegisterClient(const std::string& hostname,
                                 const std::string& ipAddress,
                                 const std::string& os,
                                 const std::string& version) {
    nlohmann::json payload = {
        {"client_id", m_clientId},
        {"hostname", hostname.empty() ? GetLocalHostname() : hostname},
        {"ip_address", ipAddress.empty() ? GetLocalIpAddress() : ipAddress},
        {"os", os.empty() ? GetLocalOsName() : os},
        {"version", version.empty() ? "1.0.0" : version}
    };

    std::string response = HttpPostJson("/api/clients/register", payload.dump());
    return !response.empty();
}

bool ServerClient::SendHeartbeat(const std::string& status) {
    nlohmann::json payload = {
        {"client_id", m_clientId},
        {"status", status.empty() ? "ONLINE" : status}
    };

    std::string response = HttpPostJson("/api/clients/heartbeat", payload.dump());
    return !response.empty();
}

bool ServerClient::SendEvent(const std::string& eventJson) {
    if (SafeTrim(eventJson).empty()) {
        return false;
    }
    std::string response = HttpPostJson("/api/events", eventJson);
    return !response.empty();
}

std::vector<USBDevice> ServerClient::FetchAllowlist() {
    std::vector<USBDevice> devices;
    std::string response = HttpGet("/api/devices");
    if (response.empty()) {
        return devices;
    }

    try {
        nlohmann::json root = nlohmann::json::parse(response);
        if (!root.is_array()) {
            return devices;
        }

        for (const auto& item : root) {
            USBDevice dev;
            if (item.contains("vid") && item["vid"].is_string()) {
                dev.vid = Utf8ToWString(item["vid"].get<std::string>());
            }
            if (item.contains("pid") && item["pid"].is_string()) {
                dev.pid = Utf8ToWString(item["pid"].get<std::string>());
            }
            if (item.contains("serial") && !item["serial"].is_null()) {
                dev.serialNumber = Utf8ToWString(item["serial"].get<std::string>());
            }
            if (item.contains("device_name") && !item["device_name"].is_null()) {
                dev.description = Utf8ToWString(item["device_name"].get<std::string>());
            }
            if (item.contains("device_type") && !item["device_type"].is_null()) {
                dev.deviceType = Utf8ToWString(item["device_type"].get<std::string>());
            }
            if (item.contains("policy") && item["policy"].is_string()) {
                dev.deviceType = Utf8ToWString(item["policy"].get<std::string>());
            }
            devices.push_back(dev);
        }
    } catch (const std::exception&) {
        return {};
    }

    return devices;
}
