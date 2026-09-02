#include "WindowsService.h"

#include "../console/ConsoleOutput.h"
#include "../runtime/ApplicationRuntime.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace {

constexpr wchar_t kServiceName[] = L"USBIPSClient";
SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
SERVICE_STATUS g_status{};
ApplicationRuntime* g_runtime = nullptr;

bool ReportStatus(DWORD state, DWORD win32ExitCode = NO_ERROR, DWORD waitHint = 0) {
    static DWORD checkpoint = 1;

    g_status.dwCurrentState = state;
    g_status.dwWin32ExitCode = win32ExitCode;
    g_status.dwWaitHint = waitHint;
    g_status.dwControlsAccepted = state == SERVICE_RUNNING
        ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN
        : 0;
    g_status.dwCheckPoint = state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING
        ? checkpoint++ : 0;

    return g_statusHandle && SetServiceStatus(g_statusHandle, &g_status) != FALSE;
}

DWORD WINAPI ServiceControlHandler(DWORD control, DWORD, void*, void*) {
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) {
        ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 30000);
        if (g_runtime) {
            g_runtime->Stop();
        }
        return NO_ERROR;
    }

    if (control == SERVICE_CONTROL_INTERROGATE) {
        ReportStatus(g_status.dwCurrentState, g_status.dwWin32ExitCode, g_status.dwWaitHint);
        return NO_ERROR;
    }

    return ERROR_CALL_NOT_IMPLEMENTED;
}

void WINAPI ServiceMain(DWORD, LPWSTR*) {
    ApplicationRuntime runtime(true);
    g_runtime = &runtime;

    g_status = {};
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwControlsAccepted = 0;
    g_status.dwCurrentState = SERVICE_START_PENDING;
    g_status.dwWin32ExitCode = NO_ERROR;
    g_status.dwWaitHint = 30000;

    g_statusHandle = RegisterServiceCtrlHandlerExW(
        kServiceName, ServiceControlHandler, nullptr);
    if (!g_statusHandle) {
        g_runtime = nullptr;
        return;
    }

    ReportStatus(SERVICE_START_PENDING, NO_ERROR, 30000);
    if (!runtime.Initialize()) {
        ReportStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR, 0);
        g_runtime = nullptr;
        return;
    }

    ReportStatus(SERVICE_RUNNING);
    runtime.Run();
    ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 30000);
    runtime.Shutdown();
    ReportStatus(SERVICE_STOPPED);
    g_runtime = nullptr;
    g_statusHandle = nullptr;
}

}

namespace WindowsService {

int Dispatch() {
    SERVICE_TABLE_ENTRYW serviceTable[] = {
        { const_cast<LPWSTR>(kServiceName), ServiceMain },
        { nullptr, nullptr }
    };

    if (StartServiceCtrlDispatcherW(serviceTable)) {
        return 0;
    }

    if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
        return 1;
    }

    ConsoleOutput::WriteLine(
        L"[service] ERROR: StartServiceCtrlDispatcherW failed.", true);
    return -1;
}

}
