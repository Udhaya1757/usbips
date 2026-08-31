#pragma once

// ============================================================
// DeviceInfoExtractor.h
// USBIPS — USB Intrusion Prevention System
// Phase 1B: Device Information Extractor
// ============================================================
// Purpose:
// Extracts rich metadata from a raw Windows device interface path.
//
// Responsibilities:
//   1. Parse Vendor ID (VID) and Product ID (PID).
//   2. Query SetupAPI for human-readable description and device class GUID.
//   3. Query Configuration Manager (CfgMgr32) for device instance ID / serial.
//   4. Query Win32 Volume APIs for drive letter and volume serial (storage).
//   5. Return a populated USBDevice structure.
// ============================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "USBDevice.h"
#include <string>

class DeviceInfoExtractor {
public:
    // ------------------------------------------------------------------
    // Extract()
    // Main extraction function. Accepts a raw device interface path
    // (e.g. from WM_DEVICECHANGE DBT_DEVICEARRIVAL) and returns a
    // fully populated USBDevice struct.
    // ------------------------------------------------------------------
    static USBDevice Extract(const std::wstring& devicePath);

    // ------------------------------------------------------------------
    // Helper parsers (exposed for unit testing or specific queries)
    // ------------------------------------------------------------------
    static std::wstring ParseVID(const std::wstring& path);
    static std::wstring ParsePID(const std::wstring& path);
    static std::wstring ParseSerialNumber(const std::wstring& path);
    static std::wstring FindDriveLetterForVolume(const std::wstring& volumePath);
    static std::wstring QueryVolumeSerialNumber(const std::wstring& driveLetter);

private:
    static bool QuerySetupApiProperties(const std::wstring& devicePath, USBDevice& outDevice);
    static std::wstring NormalizeHex4(const std::wstring& hexStr);
};
