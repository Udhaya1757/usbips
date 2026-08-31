#pragma once

// ============================================================
// USBMonitor.h
// USBIPS — USB Intrusion Prevention System
// ============================================================
// Purpose: Declares the USBMonitor class.
//
// USBMonitor owns:
//   1. A hidden Win32 window whose WNDPROC receives
//      WM_DEVICECHANGE messages from the OS kernel.
//   2. Three HDEVNOTIFY handles — one per device interface
//      GUID we care about (USB, HID, Volume/Mass-Storage).
//   3. A message loop that pumps the hidden window.
//
// The class is intentionally kept thin here.  All Windows API
// heavy-lifting lives in USBMonitor.cpp.
// ============================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN     // Exclude rarely-used Windows headers to speed up compilation
#endif

#include <Windows.h>            // Core Win32 types: HWND, HANDLE, DWORD, BOOL, …
#include <Dbt.h>                // Device-Broadcast Structures: DEV_BROADCAST_DEVICEINTERFACE,
                                //   WM_DEVICECHANGE, DBT_DEVICEARRIVAL, DBT_DEVICEREMOVECOMPLETE
#include <string>               // std::wstring for Unicode device paths
#include <array>                // std::array for the three notification handles

// ── GUIDs for the three device interface classes we monitor ──────────────────
//
// A GUID (Globally Unique Identifier) identifies a *class* of hardware.
// Windows uses GUIDs instead of strings so that matching is fast and
// unambiguous regardless of device name or driver version.
//
// We need three GUIDs because Windows notifies per *interface class*,
// not per physical port.  A single USB device can expose multiple interfaces
// (e.g., a USB hub with a Mass Storage AND a HID interface).

// {A5DCBF10-6530-11D2-901F-00C04FB951ED}
// Raw USB device-interface notifications (arrival/removal of any USB device).
// This fires for *every* USB device regardless of its function class.
static const GUID GUID_DEVINTERFACE_USB_DEVICE = {
    0xA5DCBF10, 0x6530, 0x11D2,
    { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED }
};

// {4D1E55B2-F16F-11CF-88CB-001111000030}
// Human Interface Device (HID) interface class.
// Catches keyboards, mice, gamepads, and — critically — BadUSB / Rubber Ducky
// devices that enumerate as HID keyboards to inject keystrokes.
static const GUID GUID_DEVINTERFACE_HID = {
    0x4D1E55B2, 0xF16F, 0x11CF,
    { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 }
};

// {53F5630D-B6BF-11D0-94F2-00A0C91EFB8B}
// Volume (disk/mass-storage) interface class.
// Fires when a USB flash drive or external hard disk appears as a drive volume.
// This is the interface through which data exfiltration typically happens.
static const GUID GUID_DEVINTERFACE_VOLUME = {
    0x53F5630D, 0xB6BF, 0x11D0,
    { 0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B }
};

class LocalDatabase;

// ── USBMonitor Class ──────────────────────────────────────────────────────────

class USBMonitor {
public:
    // ------------------------------------------------------------------
    // Constructor / Destructor
    // ------------------------------------------------------------------
    explicit USBMonitor(LocalDatabase* db = nullptr);
    ~USBMonitor();

    void SetDatabase(LocalDatabase* db) { m_pDb = db; }

    // ------------------------------------------------------------------
    // Start()
    // Creates the hidden message window, registers device notifications,
    // then runs the Win32 message loop until Stop() is called (e.g., via
    // Ctrl+C signal handler).
    // Returns: true if it ran to clean completion, false on init failure.
    // ------------------------------------------------------------------
    bool Start();

    // ------------------------------------------------------------------
    // Stop()
    // Posts WM_QUIT to the message loop, causing GetMessage() to return 0
    // and Start() to unwind cleanly.  Safe to call from any thread or from
    // the Ctrl+C signal handler.
    // ------------------------------------------------------------------
    void Stop();

    // ------------------------------------------------------------------
    // IsRunning()
    // Returns true while the message loop is active.
    // ------------------------------------------------------------------
    bool IsRunning() const { return m_running; }

private:
    // ------------------------------------------------------------------
    // Win32 Window Procedure (static callback)
    // ------------------------------------------------------------------
    // Why static?  Windows calls WndProc via a raw C-style function pointer
    // stored in WNDCLASSEXW::lpfnWndProc.  A non-static member function has
    // a hidden 'this' pointer and therefore cannot match that signature.
    //
    // We route back to the instance via SetWindowLongPtrW / GWLP_USERDATA
    // (see USBMonitor.cpp for the technique).
    // ------------------------------------------------------------------
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // ------------------------------------------------------------------
    // Instance-level WM_DEVICECHANGE handler
    // Called by WndProc once it has recovered the 'this' pointer.
    // ------------------------------------------------------------------
    LRESULT HandleDeviceChange(WPARAM wParam, LPARAM lParam);

    // ------------------------------------------------------------------
    // Private helpers
    // ------------------------------------------------------------------
    bool CreateMessageWindow();          // Registers window class + creates the hidden HWND
    bool RegisterDeviceNotifications(); // Calls RegisterDeviceNotificationW() for each GUID
    void UnregisterDeviceNotifications(); // Cleanup: calls UnregisterDeviceNotification()
    void DestroyMessageWindow();         // DestroyWindow() + UnregisterClassW()

    // ------------------------------------------------------------------
    // Member variables
    // ------------------------------------------------------------------
    HWND   m_hwnd;      // Handle to our hidden message-only window
    bool   m_running;   // True while the message loop is executing

    // Three notification handles — one per GUID.
    // We keep them so we can call UnregisterDeviceNotification() on exit.
    std::array<HDEVNOTIFY, 3> m_notifyHandles;

    // Name of the window class we register; kept so we can unregister it.
    static constexpr wchar_t kWindowClassName[] = L"USBIPS_Monitor_WndClass";

    LocalDatabase* m_pDb;       // Pointer to local database instance (optional)
};
