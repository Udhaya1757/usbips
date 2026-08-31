#pragma once

// ============================================================
// DeviceClassifier.h
// USBIPS — USB Intrusion Prevention System
// Phase 1C: Device Classifier
// ============================================================
// Purpose:
// Classifies a USBDevice into one of the known device categories:
//   - HID       (Keyboards, Mice, Barcode scanners, BadUSB)
//   - STORAGE   (Flash drives, External Hard Drives, SD cards)
//   - NETWORK   (Wi-Fi dongles, USB Ethernet adapters)
//   - OTHER     (Audio, Webcams, Mobile phones, Unknown peripherals)
//
// Layered Classification Strategy:
//   1. Drive Letter Detection (immediate STORAGE identification)
//   2. SetupAPI Class GUID matching
//   3. Device Interface GUID & Path matching
//   4. Keyword heuristics on Friendly Name / Description
//   5. Strict fallback to OTHER
// ============================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "../device/USBDevice.h"
#include <string>

struct ClassificationResult {
    DeviceType type;
    std::wstring typeString;    // "HID", "STORAGE", "NETWORK", "OTHER"
    std::wstring ruleName;      // Explanation of which rule triggered
};

class DeviceClassifier {
public:
    // ------------------------------------------------------------------
    // Classify()
    // Evaluates the USBDevice using multi-layered heuristic rules,
    // updates device.deviceType in-place, and returns the result details.
    // ------------------------------------------------------------------
    static ClassificationResult Classify(USBDevice& device);

private:
    static bool ContainsIgnoreCase(const std::wstring& haystack, const std::wstring& needle);
    static bool MatchGuidIgnoreCase(const std::wstring& guidStr1, const std::wstring& guidStr2);
};
