// ============================================================
// main.cpp
// USBIPS — USB Intrusion Prevention System
// Entry Point
// ============================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "runtime/ApplicationRuntime.h"
#include "service/WindowsService.h"

int main()
{
    const int dispatchResult = WindowsService::Dispatch();
    if (dispatchResult == 0) {
        return 0;
    }
    if (dispatchResult < 0) {
        return 1;
    }

    ApplicationRuntime runtime(false);
    const bool ok = runtime.Run();
    runtime.Shutdown();
    return ok ? 0 : 1;
}
