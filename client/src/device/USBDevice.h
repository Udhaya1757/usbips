#pragma once

// ============================================================
// USBDevice.h
// USBIPS — USB Intrusion Prevention System
// Phase 1B: Device Identity Structure
// ============================================================
// Purpose:
// Defines the core USBDevice data structure used across all
// subsystems (Monitor, Extractor, Classifier, Database, Access Control,
// Logging, and Network Sync).
//
// Think of this structure as the "identity card" for every USB device.
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

#include <string>
#include <iostream>
#include <iomanip>

// ── Device Type Classification ───────────────────────────────
enum class DeviceType {
    UNKNOWN,
    HID,        // Keyboards, Mice, Barcode Scanners, BadUSB
    STORAGE,    // Flash drives, External Hard Drives, SD cards
    NETWORK,    // USB Wi-Fi dongles, USB Ethernet adapters
    OTHER       // Audio, Video/Webcam, Printers, Mobile phones, etc.
};

// Convert DeviceType enum to wide string
inline std::wstring DeviceTypeToWString(DeviceType type) {
    switch (type) {
        case DeviceType::HID:     return L"HID";
        case DeviceType::STORAGE: return L"STORAGE";
        case DeviceType::NETWORK: return L"NETWORK";
        case DeviceType::OTHER:   return L"OTHER";
        default:                  return L"UNKNOWN";
    }
}

// Convert string to DeviceType enum
inline DeviceType WStringToDeviceType(const std::wstring& str) {
    if (str == L"HID")     return DeviceType::HID;
    if (str == L"STORAGE") return DeviceType::STORAGE;
    if (str == L"NETWORK") return DeviceType::NETWORK;
    if (str == L"OTHER")   return DeviceType::OTHER;
    return DeviceType::UNKNOWN;
}

// ── Core USB Device Struct ───────────────────────────────────
struct USBDevice {
    std::wstring devicePath;     // Raw Windows device interface path (\\?\USB#VID_xxxx&PID_xxxx#...)
    std::wstring vid;            // Vendor ID in uppercase hex, e.g. "0951"
    std::wstring pid;            // Product ID in uppercase hex, e.g. "1666"
    std::wstring serialNumber;   // Device hardware serial number or unique instance ID
    std::wstring deviceType;     // "HID", "STORAGE", "NETWORK", "OTHER"
    std::wstring description;    // Human-readable friendly name (e.g. "Kingston DataTraveler 3.0")
    std::wstring driveLetter;    // Logical drive letter (e.g. "E:") for mass-storage devices
    std::wstring volumeSerial;   // Volume serial number of the mounted partition (e.g. "A1B2C3D4")
    std::wstring deviceClass;    // Windows device class GUID or name string
    FILETIME firstSeen;          // Timestamp when device was first detected
    FILETIME lastSeen;           // Timestamp of most recent activity

    USBDevice()
        : deviceType(L"UNKNOWN")
    {
        firstSeen.dwLowDateTime = 0;
        firstSeen.dwHighDateTime = 0;
        lastSeen.dwLowDateTime = 0;
        lastSeen.dwHighDateTime = 0;
    }

    // Pretty-print device summary to standard wide output stream
    void PrintSummary() const {
        std::wcout
            << L"===== USB DEVICE INFORMATION =====\n"
            << L"  VID         : " << (vid.empty() ? L"(unknown)" : vid) << L"\n"
            << L"  PID         : " << (pid.empty() ? L"(unknown)" : pid) << L"\n"
            << L"  Serial      : " << (serialNumber.empty() ? L"(none/generated)" : serialNumber) << L"\n"
            << L"  Description : " << (description.empty() ? L"(unnamed device)" : description) << L"\n"
            << L"  Type        : " << (deviceType.empty() ? L"UNKNOWN" : deviceType) << L"\n"
            << L"  Device Path : " << (devicePath.empty() ? L"(none)" : devicePath) << L"\n";

        if (!driveLetter.empty()) {
            std::wcout << L"  Drive       : " << driveLetter << L"\n";
        }
        if (!volumeSerial.empty()) {
            std::wcout << L"  Volume SN   : " << volumeSerial << L"\n";
        }
        if (!deviceClass.empty()) {
            std::wcout << L"  Class GUID  : " << deviceClass << L"\n";
        }
        std::wcout << L"===================================\n";
    }
};
