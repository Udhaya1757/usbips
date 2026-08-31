// ============================================================
// DeviceInfoExtractor.cpp
// USBIPS — USB Intrusion Prevention System
// Phase 1B: Device Information Extractor
// ============================================================
//
// OVERVIEW & SECURITY SIGNIFICANCE
// ────────────────────────────────
// When a USB device is plugged in, Windows fires a notification
// containing only a raw interface path string.
// This class transforms that raw path into a structured USBDevice
// "Identity Card" by querying the Windows hardware subsystem.
//
// Why each field matters for USB security:
//   1. VID (Vendor ID)       : Identifies the hardware vendor (e.g. 0951=Kingston, 046D=Logitech).
//                              Enforces company-approved vendor policies.
//   2. PID (Product ID)      : Identifies the exact model. Differentiates a safe keyboard from
//                              a flash drive.
//   3. Serial Number         : Identifies the unique physical device. Two identical thumb drives
//                              share VID/PID, but only the corporate-provisioned unit matches
//                              the Allowlist serial.
//   4. Friendly Name/Desc    : Human-readable context for security logs and user prompts.
//   5. Device Class / Type   : Detects BadUSB/Rubber Ducky devices masquerading as keyboards.
//   6. Drive Letter / Vol SN : Identifies the specific filesystem volume for data exfiltration
//                              monitoring and removable media access controls.
//
// Key Windows APIs used:
//   - SetupAPI (SetupDi*): Queries driver/registry properties (Friendly Name, Class GUID).
//   - CfgMgr32 (CM_*): Interrogates the Windows Plug-and-Play Configuration Manager.
//   - Win32 Volume APIs: Resolves volume mount points, drive letters, and volume serials.
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
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devguid.h>

#include "DeviceInfoExtractor.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

// ── Helpers ──────────────────────────────────────────────────

static std::wstring ToUpper(std::wstring str) {
    std::transform(str.begin(), str.end(), str.begin(), ::towupper);
    return str;
}

std::wstring DeviceInfoExtractor::NormalizeHex4(const std::wstring& hexStr) {
    if (hexStr.length() == 4) return ToUpper(hexStr);
    if (hexStr.length() > 4)  return ToUpper(hexStr.substr(0, 4));
    std::wstring padded = hexStr;
    while (padded.length() < 4) padded = L"0" + padded;
    return ToUpper(padded);
}

// ── VID Extraction ───────────────────────────────────────────
// Device interface paths typically look like:
//   \\?\USB#VID_0951&PID_1666#00000000000000000D#{a5dcbf10-...}
std::wstring DeviceInfoExtractor::ParseVID(const std::wstring& path) {
    std::wstring upper = ToUpper(path);
    size_t pos = upper.find(L"VID_");
    if (pos != std::wstring::npos && pos + 8 <= upper.length()) {
        std::wstring vid = upper.substr(pos + 4, 4);
        bool allHex = true;
        for (wchar_t c : vid) {
            if (!::iswxdigit(c)) { allHex = false; break; }
        }
        if (allHex) return vid;
    }
    return L"";
}

// ── PID Extraction ───────────────────────────────────────────
std::wstring DeviceInfoExtractor::ParsePID(const std::wstring& path) {
    std::wstring upper = ToUpper(path);
    size_t pos = upper.find(L"PID_");
    if (pos != std::wstring::npos && pos + 8 <= upper.length()) {
        std::wstring pid = upper.substr(pos + 4, 4);
        bool allHex = true;
        for (wchar_t c : pid) {
            if (!::iswxdigit(c)) { allHex = false; break; }
        }
        if (allHex) return pid;
    }
    return L"";
}

// ── Serial Number Extraction ─────────────────────────────────
// In USB device interface paths, the 3rd section between `#` delimiters
// holds either the device's USB serial number or a generated instance ID:
//   \\?\USB#VID_0951&PID_1666#00000000000000000D#{...}
//                          └─────── Serial ──────┘
std::wstring DeviceInfoExtractor::ParseSerialNumber(const std::wstring& path) {
    std::vector<std::wstring> tokens;
    std::wstring token;
    for (wchar_t ch : path) {
        if (ch == L'#' || ch == L'\\') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += ch;
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }

    // Look for token that contains VID_ & PID_ and take the subsequent token
    for (size_t i = 0; i < tokens.size(); ++i) {
        std::wstring upperToken = ToUpper(tokens[i]);
        if (upperToken.find(L"VID_") != std::wstring::npos || upperToken.find(L"PID_") != std::wstring::npos) {
            if (i + 1 < tokens.size()) {
                std::wstring candidate = tokens[i + 1];
                // Strip trailing interface GUID if attached
                if (!candidate.empty() && candidate.front() == L'{') {
                    continue;
                }
                return candidate;
            }
        }
    }
    return L"";
}

// ── SetupAPI Registry Properties ─────────────────────────────
bool DeviceInfoExtractor::QuerySetupApiProperties(const std::wstring& devicePath, USBDevice& outDevice) {
    // SetupDiCreateDeviceInfoList creates an empty device information set.
    HDEVINFO hDevInfo = SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        return false;
    }

    SP_DEVICE_INTERFACE_DATA ifData = {};
    ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    // SetupDiOpenDeviceInterface adds the device interface specified by path to the set.
    bool opened = SetupDiOpenDeviceInterfaceW(hDevInfo, devicePath.c_str(), 0, &ifData);
    if (!opened) {
        // Fallback: If exact path failed (e.g. path has trailing slash), try without trailing slash
        if (!devicePath.empty() && devicePath.back() == L'\\') {
            std::wstring stripped = devicePath.substr(0, devicePath.length() - 1);
            opened = SetupDiOpenDeviceInterfaceW(hDevInfo, stripped.c_str(), 0, &ifData);
        }
    }

    if (opened) {
        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        // Retrieve the SP_DEVINFO_DATA for this interface
        if (SetupDiGetDeviceInterfaceDetailW(hDevInfo, &ifData, nullptr, 0, nullptr, &devInfoData)) {
            // SPDRP_FRIENDLYNAME: The human-readable device name given by driver/OS
            WCHAR buffer[512] = { 0 };
            DWORD regType = 0;
            DWORD reqSize = 0;

            if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME,
                                                  &regType, reinterpret_cast<PBYTE>(buffer),
                                                  sizeof(buffer), &reqSize)) {
                outDevice.description = buffer;
            } else if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, SPDRP_DEVICEDESC,
                                                         &regType, reinterpret_cast<PBYTE>(buffer),
                                                         sizeof(buffer), &reqSize)) {
                // Fallback to SPDRP_DEVICEDESC (generic description, e.g. "USB Mass Storage Device")
                outDevice.description = buffer;
            }

            // SPDRP_CLASSGUID: Windows Device Class GUID string
            if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, SPDRP_CLASSGUID,
                                                  &regType, reinterpret_cast<PBYTE>(buffer),
                                                  sizeof(buffer), &reqSize)) {
                outDevice.deviceClass = buffer;
            }

            // If serial number is not yet found, query CfgMgr32 CM_Get_Device_ID
            if (outDevice.serialNumber.empty()) {
                WCHAR devId[MAX_DEVICE_ID_LEN] = { 0 };
                if (CM_Get_Device_IDW(devInfoData.DevInst, devId, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
                    std::wstring fullDevId(devId);
                    std::wstring parsedSerial = ParseSerialNumber(fullDevId);
                    if (!parsedSerial.empty()) {
                        outDevice.serialNumber = parsedSerial;
                    }
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return true;
}

// ── Find Drive Letter for Storage Volumes ────────────────────
std::wstring DeviceInfoExtractor::FindDriveLetterForVolume(const std::wstring& volumePath) {
    // 1. If path is already a volume GUID format like "\\?\Volume{guid}\":
    WCHAR driveLetter[MAX_PATH] = { 0 };
    DWORD returnLen = 0;
    if (GetVolumePathNamesForVolumeNameW(volumePath.c_str(), driveLetter, MAX_PATH, &returnLen) && returnLen > 0) {
        std::wstring drive(driveLetter);
        if (drive.length() >= 2 && drive[1] == L':') {
            return drive.substr(0, 2);
        }
    }

    // 2. Iterate logical drives A: to Z: and match device mount paths
    DWORD driveMask = GetLogicalDrives();
    for (wchar_t letter = L'A'; letter <= L'Z'; ++letter) {
        if (driveMask & (1 << (letter - L'A'))) {
            std::wstring driveName = std::wstring(1, letter) + L":";
            WCHAR targetPath[MAX_PATH] = { 0 };
            if (QueryDosDeviceW(driveName.c_str(), targetPath, MAX_PATH)) {
                std::wstring dosTarget(targetPath);
                // Check if volume path or Dos target match
                if (!volumePath.empty() && (volumePath.find(dosTarget) != std::wstring::npos ||
                                           dosTarget.find(volumePath) != std::wstring::npos)) {
                    return driveName;
                }
            }
        }
    }

    return L"";
}

// ── Query Volume Serial Number ───────────────────────────────
std::wstring DeviceInfoExtractor::QueryVolumeSerialNumber(const std::wstring& driveLetter) {
    if (driveLetter.empty()) return L"";

    std::wstring rootPath = driveLetter;
    if (rootPath.back() != L'\\') {
        rootPath += L"\\";
    }

    DWORD volumeSerialNumber = 0;
    DWORD maxComponentLength = 0;
    DWORD fileSystemFlags = 0;
    WCHAR volumeNameBuffer[256] = { 0 };
    WCHAR fileSystemNameBuffer[256] = { 0 };

    if (GetVolumeInformationW(
            rootPath.c_str(),
            volumeNameBuffer, sizeof(volumeNameBuffer) / sizeof(WCHAR),
            &volumeSerialNumber,
            &maxComponentLength,
            &fileSystemFlags,
            fileSystemNameBuffer, sizeof(fileSystemNameBuffer) / sizeof(WCHAR))) {

        std::wstringstream ss;
        ss << std::uppercase << std::hex << std::setw(8) << std::setfill(L'0') << volumeSerialNumber;
        return ss.str();
    }

    return L"";
}

// ── Main Extract Function ────────────────────────────────────
USBDevice DeviceInfoExtractor::Extract(const std::wstring& devicePath) {
    USBDevice device;
    device.devicePath = devicePath;

    // 1. Get current system timestamp
    GetSystemTimeAsFileTime(&device.firstSeen);
    device.lastSeen = device.firstSeen;

    // 2. Parse VID and PID from device path string
    device.vid = ParseVID(devicePath);
    device.pid = ParsePID(devicePath);

    // 3. Parse Serial Number from path
    device.serialNumber = ParseSerialNumber(devicePath);

    // 4. Query SetupAPI / CfgMgr32 for registry metadata
    QuerySetupApiProperties(devicePath, device);

    // 5. Check if this is a storage volume interface and resolve drive info
    std::wstring upperPath = ToUpper(devicePath);
    if (upperPath.find(L"VOLUME") != std::wstring::npos || upperPath.find(L"STORAGE") != std::wstring::npos) {
        device.driveLetter = FindDriveLetterForVolume(devicePath);
        if (!device.driveLetter.empty()) {
            device.volumeSerial = QueryVolumeSerialNumber(device.driveLetter);
        }
        device.deviceType = L"STORAGE";
    } else if (upperPath.find(L"HID") != std::wstring::npos) {
        device.deviceType = L"HID";
    } else if (upperPath.find(L"NET") != std::wstring::npos) {
        device.deviceType = L"NETWORK";
    } else {
        device.deviceType = L"OTHER";
    }

    return device;
}
