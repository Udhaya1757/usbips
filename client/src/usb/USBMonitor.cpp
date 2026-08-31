// ============================================================
// USBMonitor.cpp
// USBIPS — USB Intrusion Prevention System
// Phase 1A: USB Device Arrival / Removal Monitor
// ============================================================
//
// OVERVIEW OF THE MECHANISM
// ─────────────────────────
// Windows delivers hardware-change notifications through the Win32
// message system (WM_DEVICECHANGE).  To receive them, we need a
// window handle (HWND).  We do NOT want a visible window, so we
// create a "message-only" window (parent = HWND_MESSAGE).
//
// The flow is:
//
//   1. RegisterClassExW()         — register our custom window class
//   2. CreateWindowExW()          — create an invisible message-only window
//   3. RegisterDeviceNotificationW() × 3 — subscribe to USB / HID / Volume
//   4. GetMessage() loop          — process WM_DEVICECHANGE messages
//   5. On WM_DEVICECHANGE         — DBT_DEVICEARRIVAL or DBT_DEVICEREMOVECOMPLETE
//   6. SetConsoleCtrlHandler()    — handle Ctrl+C → call Stop() → PostQuitMessage()
//   7. Cleanup                    — UnregisterDeviceNotification / DestroyWindow
// ============================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "USBMonitor.h"
#include "../device/DeviceInfoExtractor.h"
#include "../classifier/DeviceClassifier.h"

#include <iostream>
#include <iomanip>
#include <string>

// ── Static/global pointer used by the Ctrl+C handler ─────────────────────────
//
// SetConsoleCtrlHandler requires a plain C function pointer.  We store a
// pointer to the single USBMonitor instance so the handler can call Stop().
static USBMonitor* g_pMonitor = nullptr;

// ── Ctrl+C / Ctrl+Break Console Handler ──────────────────────────────────────
//
// SetConsoleCtrlHandler() registers a callback that Windows calls on:
//   CTRL_C_EVENT        — user pressed Ctrl+C
//   CTRL_BREAK_EVENT    — user pressed Ctrl+Break
//   CTRL_CLOSE_EVENT    — user closed the console window
//   CTRL_LOGOFF_EVENT   — user is logging off (service scenario)
//   CTRL_SHUTDOWN_EVENT — system is shutting down (service scenario)
//
// Returning TRUE tells Windows "we handled it — don't propagate further."
// Returning FALSE lets Windows invoke the next handler in the chain (which
// would normally terminate the process immediately).
//
// We call Stop() which internally posts WM_QUIT to our message loop,
// causing GetMessage() to return 0 and Start() to return cleanly.
// ─────────────────────────────────────────────────────────────────────────────
static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType)
{
    switch (ctrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        std::wcout << L"\n[USBMonitor] Shutdown signal received. Stopping...\n";
        if (g_pMonitor)
            g_pMonitor->Stop();
        // Sleep long enough for the message loop thread to exit cleanly.
        // (In Phase 1K when we move to a service, this becomes more formal.)
        ::Sleep(2000);
        return TRUE;

    default:
        return FALSE;
    }
}

// ── Constructor ───────────────────────────────────────────────────────────────
USBMonitor::USBMonitor()
    : m_hwnd(nullptr)
    , m_running(false)
{
    // Zero-initialise all three HDEVNOTIFY handles so we can safely check
    // them in UnregisterDeviceNotifications() without undefined behaviour.
    m_notifyHandles.fill(nullptr);
}

// ── Destructor ────────────────────────────────────────────────────────────────
USBMonitor::~USBMonitor()
{
    // Belt-and-braces: if the caller forgets to call Stop() before destroying
    // the object, clean up the OS resources ourselves.
    UnregisterDeviceNotifications();
    DestroyMessageWindow();
}

// ============================================================
//  Start()
//  Public entry point.  Blocks until Stop() is called.
// ============================================================
bool USBMonitor::Start()
{
    // ── Store global pointer for the Ctrl+C handler ──────────────────────
    g_pMonitor = this;

    // ── Register our Ctrl+C / Ctrl+Break handler ─────────────────────────
    //
    // SetConsoleCtrlHandler(handler, TRUE) pushes our handler onto a stack.
    // Windows walks the stack newest-to-oldest until a handler returns TRUE.
    // Passing FALSE as the second argument would *remove* the handler.
    if (!::SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE))
    {
        std::wcerr << L"[USBMonitor] SetConsoleCtrlHandler() failed. GLE="
                   << ::GetLastError() << L"\n";
        return false;
    }

    // ── Create the hidden message-only window ────────────────────────────
    if (!CreateMessageWindow())
        return false;

    // ── Subscribe to device-interface notifications ──────────────────────
    if (!RegisterDeviceNotifications())
    {
        DestroyMessageWindow();
        return false;
    }

    std::wcout
        << L"========================================\n"
        << L"  USBIPS USB Monitor — Active\n"
        << L"  Watching: USB | HID | Volume\n"
        << L"  Press Ctrl+C to stop.\n"
        << L"========================================\n\n";

    m_running = true;

    // ── Win32 Message Loop ────────────────────────────────────────────────
    //
    // GetMessage() blocks until a message arrives in the thread's queue.
    // It returns:
    //   > 0  : a normal message was retrieved → dispatch it.
    //   = 0  : WM_QUIT was retrieved → exit the loop cleanly.
    //   = -1 : an error occurred → treat as fatal.
    //
    // TranslateMessage() converts raw keyboard input into WM_CHAR etc.
    // It is a no-op for non-keyboard messages but is required by the Win32
    // message loop contract — leaving it out can cause subtle keyboard bugs
    // if we later add a visible window.
    //
    // DispatchMessage() calls our WndProc with the message.
    // ─────────────────────────────────────────────────────────────────────
    MSG msg{};
    BOOL bRet;

    while ((bRet = ::GetMessage(&msg, nullptr, 0, 0)) != 0)
    {
        if (bRet == -1)
        {
            // GetMessage() itself failed — extremely rare.
            std::wcerr << L"[USBMonitor] GetMessage() error. GLE="
                       << ::GetLastError() << L"\n";
            break;
        }
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    m_running = false;

    // ── Cleanup ───────────────────────────────────────────────────────────
    UnregisterDeviceNotifications();
    DestroyMessageWindow();

    // Deregister our Ctrl+C handler (second param FALSE = remove).
    ::SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE);
    g_pMonitor = nullptr;

    std::wcout << L"\n[USBMonitor] Stopped cleanly.\n";
    return true;
}

// ============================================================
//  Stop()
//  Thread-safe: PostQuitMessage / PostThreadMessage can be called
//  from any thread, including the Ctrl+C handler thread.
// ============================================================
void USBMonitor::Stop()
{
    if (m_hwnd)
    {
        // PostQuitMessage(0) places WM_QUIT into the queue of the thread
        // that *created* m_hwnd.  This causes GetMessage() to return 0.
        //
        // Why not SendMessage()?  SendMessage() blocks until WndProc returns.
        // PostQuitMessage() is asynchronous and safe from any thread.
        ::PostQuitMessage(0);
    }
}

// ============================================================
//  CreateMessageWindow()
//  Registers a minimal WNDCLASSEXW and creates a message-only window.
// ============================================================
bool USBMonitor::CreateMessageWindow()
{
    // ── Step 1: Fill WNDCLASSEXW ─────────────────────────────────────────
    //
    // WNDCLASSEXW describes the "template" for windows of this class.
    // Every window that receives WM_DEVICECHANGE needs a window class.
    //
    // Key fields:
    //   cbSize        — must always be sizeof(WNDCLASSEXW); Win32 uses it
    //                   to distinguish this struct from the older WNDCLASS.
    //   style         — CS_OWNDC gives us a private device context (not needed
    //                   here but harmless; 0 is also fine for a message-only window).
    //   lpfnWndProc   — pointer to our static WndProc callback.
    //   hInstance     — the module instance (our .exe) that owns this class.
    //   lpszClassName — the name we use to reference this class in CreateWindow.
    // ─────────────────────────────────────────────────────────────────────
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = USBMonitor::WndProc;
    wc.hInstance     = ::GetModuleHandleW(nullptr);  // GetModuleHandleW(nullptr) = handle
                                                      // to the currently running .exe
    wc.lpszClassName = kWindowClassName;

    // ── Step 2: RegisterClassExW() ───────────────────────────────────────
    //
    // Registers our window class with the Win32 subsystem.
    // Returns an ATOM (a 16-bit integer ID) on success, 0 on failure.
    // The class remains registered until we call UnregisterClassW() or
    // our process exits.
    if (!::RegisterClassExW(&wc))
    {
        DWORD gle = ::GetLastError();
        // ERROR_CLASS_ALREADY_EXISTS (1410) is benign — it means we called
        // this function twice (e.g., in unit-test scenarios).
        if (gle != ERROR_CLASS_ALREADY_EXISTS)
        {
            std::wcerr << L"[USBMonitor] RegisterClassExW() failed. GLE=" << gle << L"\n";
            return false;
        }
    }

    // ── Step 3: CreateWindowExW() ────────────────────────────────────────
    //
    // Creates the actual window.  Normally a window is visible on screen,
    // but setting hWndParent = HWND_MESSAGE creates a "message-only window":
    //   • It is invisible and has no screen position.
    //   • It can still receive posted/sent messages — including WM_DEVICECHANGE.
    //   • It consumes no desktop resources.
    //   • It cannot be a child of a normal window; only of HWND_MESSAGE or nullptr.
    //
    // Parameter-by-parameter:
    //   dwExStyle    = 0            — no extended styles needed
    //   lpClassName  = kWindowClassName — the class we just registered
    //   lpWindowName = L"USBIPS"    — window title (not displayed; for debug only)
    //   dwStyle      = 0            — no visible style flags
    //   X, Y, W, H   = 0,0,0,0     — position/size irrelevant for message-only windows
    //   hWndParent   = HWND_MESSAGE — THIS IS THE KEY: makes it message-only
    //   hMenu        = nullptr      — no menu
    //   hInstance    = our .exe handle
    //   lpParam      = this         — we pass 'this' so WndProc can recover it via
    //                                 CREATESTRUCT.lpCreateParams → SetWindowLongPtrW
    // ─────────────────────────────────────────────────────────────────────
    m_hwnd = ::CreateWindowExW(
        0,                          // dwExStyle
        kWindowClassName,           // lpClassName
        L"USBIPS_Monitor",          // lpWindowName  (hidden, for debugging only)
        0,                          // dwStyle
        0, 0, 0, 0,                 // X, Y, nWidth, nHeight
        HWND_MESSAGE,               // hWndParent — message-only window
        nullptr,                    // hMenu
        ::GetModuleHandleW(nullptr), // hInstance
        this                        // lpParam — passed to WM_CREATE as CREATESTRUCT::lpCreateParams
    );

    if (!m_hwnd)
    {
        std::wcerr << L"[USBMonitor] CreateWindowExW() failed. GLE="
                   << ::GetLastError() << L"\n";
        return false;
    }

    return true;
}

// ============================================================
//  RegisterDeviceNotifications()
//  Calls RegisterDeviceNotificationW() for each GUID we care about.
// ============================================================
bool USBMonitor::RegisterDeviceNotifications()
{
    // ── DEV_BROADCAST_DEVICEINTERFACE ────────────────────────────────────
    //
    // This structure tells Windows *which* device-interface class to watch.
    //
    // Fields:
    //   dbcc_size       — size of the structure in bytes (mandatory).
    //   dbcc_devicetype — must be DBT_DEVTYP_DEVICEINTERFACE for interface
    //                     notifications (as opposed to DBT_DEVTYP_HANDLE for
    //                     per-handle notifications or DBT_DEVTYP_VOLUME for
    //                     logical volumes — though we use GUID_DEVINTERFACE_VOLUME
    //                     which still uses DBT_DEVTYP_DEVICEINTERFACE).
    //   dbcc_classguid  — the GUID of the interface class to monitor.
    //   dbcc_name       — filled in by Windows on notification; unused here.
    // ─────────────────────────────────────────────────────────────────────
    DEV_BROADCAST_DEVICEINTERFACE filter{};
    filter.dbcc_size       = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    // ── RegisterDeviceNotificationW() ────────────────────────────────────
    //
    // Registers our window to receive WM_DEVICECHANGE for a specific
    // device interface class.
    //
    // Parameters:
    //   hRecipient   — our window handle (HWND).  Windows will POST
    //                  WM_DEVICECHANGE to this window when the class
    //                  of interest changes.
    //   NotificationFilter — pointer to our DEV_BROADCAST_DEVICEINTERFACE.
    //   Flags        — DEVICE_NOTIFY_WINDOW_HANDLE means hRecipient is an HWND.
    //                  (Alternative: DEVICE_NOTIFY_SERVICE_HANDLE for services.)
    //
    // Returns HDEVNOTIFY (an opaque handle) on success, nullptr on failure.
    // We must keep the handle and call UnregisterDeviceNotification() on exit,
    // otherwise Windows continues delivering notifications to our (destroyed)
    // window — a resource leak and potential crash.
    // ─────────────────────────────────────────────────────────────────────

    const GUID guids[3] = {
        GUID_DEVINTERFACE_USB_DEVICE,
        GUID_DEVINTERFACE_HID,
        GUID_DEVINTERFACE_VOLUME
    };
    const wchar_t* guidNames[3] = {
        L"USB_DEVICE",
        L"HID",
        L"VOLUME"
    };

    for (int i = 0; i < 3; ++i)
    {
        filter.dbcc_classguid = guids[i];

        m_notifyHandles[i] = ::RegisterDeviceNotificationW(
            m_hwnd,
            &filter,
            DEVICE_NOTIFY_WINDOW_HANDLE
        );

        if (!m_notifyHandles[i])
        {
            std::wcerr << L"[USBMonitor] RegisterDeviceNotificationW() failed for "
                       << guidNames[i] << L". GLE=" << ::GetLastError() << L"\n";
            // Unregister any handles already registered before this failure.
            UnregisterDeviceNotifications();
            return false;
        }

        std::wcout << L"[USBMonitor] Registered notification for: " << guidNames[i] << L"\n";
    }

    return true;
}

// ============================================================
//  UnregisterDeviceNotifications()
//  Releases all three HDEVNOTIFY handles.
// ============================================================
void USBMonitor::UnregisterDeviceNotifications()
{
    for (auto& handle : m_notifyHandles)
    {
        if (handle)
        {
            // UnregisterDeviceNotification() tells Windows to stop delivering
            // WM_DEVICECHANGE for this subscription.  Must be called before
            // the window is destroyed; otherwise we'd have a dangling HWND
            // registered with the device notification system.
            ::UnregisterDeviceNotification(handle);
            handle = nullptr;
        }
    }
}

// ============================================================
//  DestroyMessageWindow()
//  Tears down the HWND and unregisters the window class.
// ============================================================
void USBMonitor::DestroyMessageWindow()
{
    if (m_hwnd)
    {
        // DestroyWindow() sends WM_DESTROY + WM_NCDESTROY to the window,
        // then removes it from the window manager.  After this call m_hwnd
        // is invalid and must not be used.
        ::DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    // UnregisterClassW() frees the WNDCLASSEXW registration we made in
    // RegisterClassExW().  Failing to do this leaks a small kernel structure
    // for the lifetime of the process.
    ::UnregisterClassW(kWindowClassName, ::GetModuleHandleW(nullptr));
}

// ============================================================
//  WndProc()  — static window procedure (message callback)
//
//  Windows calls this for EVERY message sent/posted to m_hwnd.
//  Because it is static, it has no implicit 'this' pointer.
//  We recover the instance pointer via GWLP_USERDATA (set during
//  WM_CREATE below) and forward to HandleDeviceChange().
// ============================================================
LRESULT CALLBACK USBMonitor::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // ── WM_CREATE ────────────────────────────────────────────────────────
    //
    // WM_CREATE is the very first message delivered to a new window,
    // sent synchronously during CreateWindowExW() before it returns.
    //
    // lParam points to a CREATESTRUCTW whose lpCreateParams field holds
    // whatever we passed as the last argument to CreateWindowExW() —
    // in our case, the 'this' pointer of the USBMonitor instance.
    //
    // SetWindowLongPtrW(hwnd, GWLP_USERDATA, ptr) stores an arbitrary
    // pointer in a per-window slot maintained by Win32.  This is the
    // canonical pattern for associating C++ objects with HWND values.
    // ─────────────────────────────────────────────────────────────────────
    if (msg == WM_CREATE)
    {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        USBMonitor*    pThis   = reinterpret_cast<USBMonitor*>(pCreate->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        return 0;
    }

    // ── Recover 'this' pointer for all other messages ─────────────────────
    //
    // GetWindowLongPtrW(hwnd, GWLP_USERDATA) retrieves what we stored above.
    // Note: before WM_CREATE fires (e.g., for WM_GETMINMAXINFO which Windows
    // may send even earlier), pThis will be nullptr — we guard against that.
    USBMonitor* pThis = reinterpret_cast<USBMonitor*>(
        ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    // ── WM_DEVICECHANGE ──────────────────────────────────────────────────
    //
    // This is the core message for our entire monitor.
    //
    // wParam encodes the event type:
    //   DBT_DEVICEARRIVAL        (0x8000) — a device or piece of media has been inserted.
    //   DBT_DEVICEREMOVECOMPLETE (0x8004) — device removal is complete.
    //   DBT_DEVNODES_CHANGED     (0x0007) — the device tree changed (no lParam data).
    //   (and others we don't need to handle here)
    //
    // lParam points to a DEV_BROADCAST_HDR identifying which device triggered
    // the event.  Its dbch_devicetype field tells us how to interpret the rest
    // of the structure (e.g., cast to DEV_BROADCAST_DEVICEINTERFACE for our case).
    // ─────────────────────────────────────────────────────────────────────
    if (msg == WM_DEVICECHANGE && pThis)
        return pThis->HandleDeviceChange(wParam, lParam);

    // ── WM_DESTROY ───────────────────────────────────────────────────────
    //
    // Sent when DestroyWindow() is called.  We don't need to do extra
    // cleanup here because we do it in DestroyMessageWindow() / destructor,
    // but we still pass it to DefWindowProcW for correctness.
    // ─────────────────────────────────────────────────────────────────────

    // DefWindowProcW provides default handling for all messages we don't
    // explicitly handle.  Always call it for unhandled messages.
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================
//  HandleDeviceChange()
//  Instance method that decodes WM_DEVICECHANGE payloads.
// ============================================================
LRESULT USBMonitor::HandleDeviceChange(WPARAM wParam, LPARAM lParam)
{
    // DBT_DEVNODES_CHANGED has no lParam structure — just a signal that
    // the device tree changed.  We ignore it here (higher-detail events
    // are handled below).
    if (wParam == DBT_DEVNODES_CHANGED)
        return TRUE;

    // For DBT_DEVICEARRIVAL and DBT_DEVICEREMOVECOMPLETE, lParam is a
    // pointer to a DEV_BROADCAST_HDR.  We inspect dbch_devicetype to
    // verify it is the type we registered for (DBT_DEVTYP_DEVICEINTERFACE),
    // then cast to DEV_BROADCAST_DEVICEINTERFACE to read the device path.
    if (!lParam)
        return TRUE;

    auto* pHdr = reinterpret_cast<DEV_BROADCAST_HDR*>(lParam);
    if (pHdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
        return TRUE;   // Not an interface-class event; ignore.

    // Cast to the full structure to access dbcc_name (the device interface path).
    // dbcc_name is a null-terminated wide string like:
    //   \\?\USB#VID_0781&PID_5581#...#{a5dcbf10-6530-11d2-901f-00c04fb951ed}
    // This path can later be passed to SetupAPI / CfgMgr32 to enumerate
    // device properties (VID, PID, serial number, driver info, etc.).
    auto* pDev = reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE_W*>(pHdr);
    std::wstring devicePath(pDev->dbcc_name);

    // ── DBT_DEVICEARRIVAL ────────────────────────────────────────────────
    if (wParam == DBT_DEVICEARRIVAL)
    {
        std::wcout
            << L"\n========================================\n"
            << L"  USB DEVICE CONNECTED\n"
            << L"========================================\n";

        // Phase 1B: Extract rich device identity using SetupAPI, CfgMgr32 & Volume APIs
        USBDevice device = DeviceInfoExtractor::Extract(devicePath);

        // Phase 1C: Classify device category using layered heuristics
        ClassificationResult classification = DeviceClassifier::Classify(device);

        device.PrintSummary();
        std::wcout << L"[CLASSIFIER] Device type: " << classification.typeString
                   << L" (rule: " << classification.ruleName << L")\n\n";

        // TODO (Phase 1E): Pass device to AccessController (Allowlist check / Enforce policy)
    }
    // ── DBT_DEVICEREMOVECOMPLETE ─────────────────────────────────────────
    else if (wParam == DBT_DEVICEREMOVECOMPLETE)
    {
        std::wcout
            << L"\n========================================\n"
            << L"  USB DEVICE REMOVED\n";

        std::wstring vid = DeviceInfoExtractor::ParseVID(devicePath);
        std::wstring pid = DeviceInfoExtractor::ParsePID(devicePath);
        std::wstring serial = DeviceInfoExtractor::ParseSerialNumber(devicePath);

        if (!vid.empty() && !pid.empty()) {
            std::wcout << L"  Device: VID_" << vid << L"&PID_" << pid;
            if (!serial.empty()) {
                std::wcout << L" (SN: " << serial << L")";
            }
            std::wcout << L"\n";
        }
        std::wcout
            << L"  Device Interface: " << devicePath << L"\n"
            << L"========================================\n";

        // TODO (Phase 1F): Log disconnection event via Logger module
    }

    return TRUE;
}
