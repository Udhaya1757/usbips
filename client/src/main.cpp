// ============================================================
// main.cpp
// USBIPS — USB Intrusion Prevention System
// Entry Point
// ============================================================
//
// This is the entry point for the USBIPS client console application.
//
// Responsibilities:
//   1. Set the console to UTF-16 (wide) output so device path strings
//      with Unicode characters print correctly.
//   2. Instantiate USBMonitor.
//   3. Call monitor.Start(), which blocks until Ctrl+C is pressed.
//
// In Phase 1K (Windows Service), this main() will be replaced by a
// ServiceMain() entry point, but the USBMonitor class itself stays
// unchanged — it just runs on the service thread instead.
// ============================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>   // For SetConsoleOutputCP / GetConsoleWindow (optional use)
#include <iostream>
#include <io.h>        // _setmode
#include <fcntl.h>     // _O_U16TEXT

#include "usb/USBMonitor.h"
#include "database/LocalDatabase.h"
#include "access/AccessController.h"

int main()
{
    // ── Configure console for wide-character (UTF-16) output ─────────────
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    std::wcout
        << L"============================================================\n"
        << L"  USBIPS — USB Intrusion Prevention System\n"
        << L"  Client v0.1 — Phase 1: USB Monitor & Access Controller\n"
        << L"============================================================\n\n";

    // ── Initialize Local SQLite Database ──────────────────────────────────
    LocalDatabase db;
    if (!db.Initialize("usbips_local.db")) {
        std::wcout << L"[main] WARNING: Failed to initialize local SQLite database.\n"
                   << L"       Allowlist matching and local event logging will be disabled.\n\n";
    } else {
        std::wcout << L"[main] Local SQLite database initialized: usbips_local.db\n\n";
    }

    // ── Initialize Access Controller Engine ────────────────────────────────
    AccessController accessController(&db);

    // ── Create the USBMonitor instance ────────────────────────────────────
    USBMonitor monitor(&db, &accessController);

    // ── Start monitoring ──────────────────────────────────────────────────
    //
    // Start() does, in order:
    //   1. SetConsoleCtrlHandler()        — register Ctrl+C handler
    //   2. RegisterClassExW()             — register our hidden window class
    //   3. CreateWindowExW(HWND_MESSAGE)  — create the invisible message window
    //   4. RegisterDeviceNotificationW()  — subscribe to USB/HID/Volume events
    //   5. GetMessage() loop              — pump messages until WM_QUIT
    //   6. Cleanup on exit
    //
    // The call blocks here until the user presses Ctrl+C or closes the console.
    bool ok = monitor.Start();

    std::wcout << L"\n[main] USBMonitor exited " << (ok ? L"cleanly." : L"with errors.") << L"\n";

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
