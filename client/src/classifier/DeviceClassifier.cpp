// ============================================================
// DeviceClassifier.cpp
// USBIPS — USB Intrusion Prevention System
// Phase 1C: Device Classifier Implementation
// ============================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "DeviceClassifier.h"
#include <algorithm>
#include <cwctype>
#include <iostream>

// Standard Windows Device Class GUIDs
static const wchar_t* GUID_STR_DEVCLASS_HIDCLASS   = L"{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}"; // Human Interface Devices
static const wchar_t* GUID_STR_DEVCLASS_DISKDRIVE  = L"{4D36E967-E325-11CE-BFC1-08002BE10318}"; // Disk Drives / Flash
static const wchar_t* GUID_STR_DEVCLASS_VOLUME     = L"{71A27CDD-812A-11D0-BEC7-08002BE2092F}"; // Volume / Storage Partitions
static const wchar_t* GUID_STR_DEVCLASS_NET        = L"{4D36E972-E325-11CE-BFC1-08002BE10318}"; // Network Adapters
static const wchar_t* GUID_STR_DEVCLASS_USB        = L"{36FC9E60-C465-11CF-8056-444553540000}"; // USB Controllers & Devices

// Standard Windows Device Interface GUIDs
static const wchar_t* GUID_STR_DEVINTERFACE_HID    = L"{4D1E55B2-F16F-11CF-88CB-001111000030}";
static const wchar_t* GUID_STR_DEVINTERFACE_VOLUME = L"{53F5630D-B6BF-11D0-94F2-00A0C91EFB8B}";
static const wchar_t* GUID_STR_DEVINTERFACE_NET    = L"{AD498944-762F-11D0-8DCB-00C04FC335E3}";

bool DeviceClassifier::ContainsIgnoreCase(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    if (haystack.length() < needle.length()) return false;

    std::wstring h = haystack;
    std::wstring n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::towupper);
    std::transform(n.begin(), n.end(), n.begin(), ::towupper);

    return h.find(n) != std::wstring::npos;
}

bool DeviceClassifier::MatchGuidIgnoreCase(const std::wstring& guidStr1, const std::wstring& guidStr2) {
    if (guidStr1.empty() || guidStr2.empty()) return false;
    return ContainsIgnoreCase(guidStr1, guidStr2) || ContainsIgnoreCase(guidStr2, guidStr1);
}

ClassificationResult DeviceClassifier::Classify(USBDevice& device) {
    ClassificationResult result;
    result.type = DeviceType::OTHER;
    result.typeString = L"OTHER";
    result.ruleName = L"default fallback";

    // ── Rule 1: Drive Letter Assigned ────────────────────────────
    // If Windows has already mounted a logical drive letter (e.g. E:),
    // it is definitively a Mass Storage device.
    if (!device.driveLetter.empty()) {
        result.type = DeviceType::STORAGE;
        result.typeString = L"STORAGE";
        result.ruleName = L"drive letter assigned (" + device.driveLetter + L")";
        device.deviceType = result.typeString;
        return result;
    }

    // ── Rule 2: Windows Device Class GUID Match ──────────────────
    if (!device.deviceClass.empty()) {
        if (MatchGuidIgnoreCase(device.deviceClass, GUID_STR_DEVCLASS_HIDCLASS)) {
            result.type = DeviceType::HID;
            result.typeString = L"HID";
            result.ruleName = L"device class matches GUID_DEVCLASS_HIDCLASS";
            device.deviceType = result.typeString;
            return result;
        }

        if (MatchGuidIgnoreCase(device.deviceClass, GUID_STR_DEVCLASS_DISKDRIVE) ||
            MatchGuidIgnoreCase(device.deviceClass, GUID_STR_DEVCLASS_VOLUME)) {
            result.type = DeviceType::STORAGE;
            result.typeString = L"STORAGE";
            result.ruleName = L"device class matches DISKDRIVE/VOLUME GUID";
            device.deviceType = result.typeString;
            return result;
        }

        if (MatchGuidIgnoreCase(device.deviceClass, GUID_STR_DEVCLASS_NET)) {
            result.type = DeviceType::NETWORK;
            result.typeString = L"NETWORK";
            result.ruleName = L"device class matches GUID_DEVCLASS_NET";
            device.deviceType = result.typeString;
            return result;
        }
    }

    // ── Rule 3: Interface Path & Interface GUID Matching ─────────
    if (!device.devicePath.empty()) {
        if (ContainsIgnoreCase(device.devicePath, L"HID") ||
            ContainsIgnoreCase(device.devicePath, GUID_STR_DEVINTERFACE_HID)) {
            result.type = DeviceType::HID;
            result.typeString = L"HID";
            result.ruleName = L"device path matches HID interface";
            device.deviceType = result.typeString;
            return result;
        }

        if (ContainsIgnoreCase(device.devicePath, L"STORAGE") ||
            ContainsIgnoreCase(device.devicePath, L"VOLUME") ||
            ContainsIgnoreCase(device.devicePath, GUID_STR_DEVINTERFACE_VOLUME)) {
            result.type = DeviceType::STORAGE;
            result.typeString = L"STORAGE";
            result.ruleName = L"device path matches STORAGE/VOLUME interface";
            device.deviceType = result.typeString;
            return result;
        }

        if (ContainsIgnoreCase(device.devicePath, L"NET") ||
            ContainsIgnoreCase(device.devicePath, GUID_STR_DEVINTERFACE_NET)) {
            result.type = DeviceType::NETWORK;
            result.typeString = L"NETWORK";
            result.ruleName = L"device path matches NETWORK interface";
            device.deviceType = result.typeString;
            return result;
        }
    }

    // ── Rule 4: Friendly Name / Description Heuristics ───────────
    if (!device.description.empty()) {
        // HID Keywords
        if (ContainsIgnoreCase(device.description, L"keyboard") ||
            ContainsIgnoreCase(device.description, L"mouse") ||
            ContainsIgnoreCase(device.description, L"touchpad") ||
            ContainsIgnoreCase(device.description, L"pointer") ||
            ContainsIgnoreCase(device.description, L"barcode") ||
            ContainsIgnoreCase(device.description, L"joystick") ||
            ContainsIgnoreCase(device.description, L"gamepad")) {
            result.type = DeviceType::HID;
            result.typeString = L"HID";
            result.ruleName = L"description matches HID keywords";
            device.deviceType = result.typeString;
            return result;
        }

        // Network Keywords
        if (ContainsIgnoreCase(device.description, L"wi-fi") ||
            ContainsIgnoreCase(device.description, L"wifi") ||
            ContainsIgnoreCase(device.description, L"wireless") ||
            ContainsIgnoreCase(device.description, L"ethernet") ||
            ContainsIgnoreCase(device.description, L"network") ||
            ContainsIgnoreCase(device.description, L"802.11") ||
            ContainsIgnoreCase(device.description, L"lan")) {
            result.type = DeviceType::NETWORK;
            result.typeString = L"NETWORK";
            result.ruleName = L"description matches NETWORK keywords";
            device.deviceType = result.typeString;
            return result;
        }

        // Storage Keywords
        if (ContainsIgnoreCase(device.description, L"flash") ||
            ContainsIgnoreCase(device.description, L"drive") ||
            ContainsIgnoreCase(device.description, L"disk") ||
            ContainsIgnoreCase(device.description, L"storage") ||
            ContainsIgnoreCase(device.description, L"mass storage") ||
            ContainsIgnoreCase(device.description, L"sd card") ||
            ContainsIgnoreCase(device.description, L"datatraveler") ||
            ContainsIgnoreCase(device.description, L"cruzer") ||
            ContainsIgnoreCase(device.description, L"sandisk") ||
            ContainsIgnoreCase(device.description, L"kingston")) {
            result.type = DeviceType::STORAGE;
            result.typeString = L"STORAGE";
            result.ruleName = L"description matches STORAGE keywords";
            device.deviceType = result.typeString;
            return result;
        }
    }

    // ── Fallback ─────────────────────────────────────────────────
    device.deviceType = result.typeString;
    return result;
}
