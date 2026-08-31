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

int main()
{
    // ── Configure console for wide-character (UTF-16) output ─────────────
    //
    // By default, Windows console streams are in narrow-char ANSI mode.
    // Device interface paths returned by WM_DEVICECHANGE are wide strings
    // (wchar_t / WCHAR) and may contain non-ASCII characters.
    //
    // _setmode(_fileno(stdout), _O_U16TEXT) switches stdout to UTF-16LE
    // mode so that std::wcout outputs wide strings correctly.
    // Without this, every std::wcout call may produce garbled or empty output.
    //
    // Note: After _O_U16TEXT, do NOT mix narrow std::cout with std::wcout
    // on the same stream — it will assert in debug builds.
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    std::wcout
        << L"============================================================\n"
        << L"  USBIPS — USB Intrusion Prevention System\n"
        << L"  Client v0.1 — Phase 1A: USB Device Monitor\n"
        << L"============================================================\n\n";

    // ── Create the USBMonitor instance ────────────────────────────────────
    //
    // USBMonitor is a RAII object.  Its constructor is lightweight (no Win32
    // calls yet).  All initialization happens inside Start().
    USBMonitor monitor;

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
