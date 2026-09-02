#include "ApplicationRuntime.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "../access/AccessController.h"
#include "../client/ClientManager.h"
#include "../console/ConsoleOutput.h"
#include "../database/LocalDatabase.h"
#include "../logging/EventLogger.h"
#include "../network/ServerClient.h"
#include "../usb/USBMonitor.h"

#include <cstdlib>

ApplicationRuntime::ApplicationRuntime(bool serviceMode)
    : m_serviceMode(serviceMode) {
}

ApplicationRuntime::~ApplicationRuntime() {
    Shutdown();
}

std::chrono::seconds ApplicationRuntime::GetHeartbeatInterval() {
    const char* configuredInterval = std::getenv("USBIPS_HEARTBEAT_INTERVAL_SECONDS");
    if (!configuredInterval) {
        return std::chrono::seconds(60);
    }

    try {
        long intervalSeconds = std::stol(configuredInterval);
        if (intervalSeconds > 0) {
            return std::chrono::seconds(intervalSeconds);
        }
    } catch (const std::exception&) {
    }

    return std::chrono::seconds(60);
}

bool ApplicationRuntime::InitializeServiceDataDirectory() {
    const char* programData = std::getenv("ProgramData");
    std::wstring baseDirectory = programData && *programData
        ? LocalDatabase::ToWide(programData)
        : L"C:\\ProgramData";

    m_serviceDataDirectory = baseDirectory + L"\\USBIPS";
    if (::CreateDirectoryW(m_serviceDataDirectory.c_str(), nullptr)) {
        return true;
    }

    return ::GetLastError() == ERROR_ALREADY_EXISTS;
}

std::string ApplicationRuntime::GetDataPath(const std::string& fileName) const {
    if (!m_serviceMode) {
        return fileName;
    }

    return LocalDatabase::ToUtf8(m_serviceDataDirectory) + "\\" + fileName;
}

bool ApplicationRuntime::Initialize() {
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    if (m_initialized) {
        return true;
    }

    ConsoleOutput::Install();

    ConsoleOutput::Write(
        L"============================================================\n"
        L"  USBIPS — USB Intrusion Prevention System\n"
        L"  Client v0.1 — Phase 1: USB Monitor & Access Controller\n"
        L"============================================================\n\n");

    if (m_serviceMode && !InitializeServiceDataDirectory()) {
        ConsoleOutput::WriteLine(L"[runtime] ERROR: Failed to create %ProgramData%\\USBIPS.", true);
        return false;
    }

    m_database = std::make_unique<LocalDatabase>();
    if (!m_database->Initialize(GetDataPath("usbips_local.db"))) {
        ConsoleOutput::WriteLine(L"[main] WARNING: Failed to initialize local SQLite database.");
        ConsoleOutput::WriteLine(L"       Allowlist matching and local event logging will be disabled.");
        if (m_serviceMode) {
            return false;
        }
    } else {
        ConsoleOutput::WriteLine(L"[main] Local SQLite database initialized: usbips_local.db");
    }

    m_eventLogger = std::make_unique<EventLogger>();
    if (!m_eventLogger->Initialize(
            m_database.get(),
            GetDataPath("usbips_events.log"),
            GetDataPath("usbips_client.conf"))) {
        ConsoleOutput::WriteLine(L"[main] WARNING: Failed to initialize event logger.");
        if (m_serviceMode) {
            return false;
        }
    } else {
        ConsoleOutput::WriteLine(L"[main] Event logger initialized: usbips_events.log");
        ConsoleOutput::Write(L"[main] Client ID: " + LocalDatabase::ToWide(m_eventLogger->GetClientId()) + L"\n\n");
    }

    m_serverClient = std::make_unique<ServerClient>(
        "http://127.0.0.1:8000", m_eventLogger->GetClientId());
    m_accessController = std::make_unique<AccessController>(
        m_database.get(), m_serverClient.get());
    m_accessController->SetInteractivePrompts(!m_serviceMode);

    if (!m_eventLogger->GetClientId().empty()) {
        m_serverClient->RegisterClient("USBIPS-CLIENT", "127.0.0.1", "Windows", "1.0.0");
    }

    m_clientManager = std::make_unique<ClientManager>(
        m_serverClient.get(), GetHeartbeatInterval());
    if (!m_clientManager->Start()) {
        ConsoleOutput::WriteLine(L"[runtime] WARNING: Client heartbeat manager could not start.");
    }

    m_usbMonitor = std::make_unique<USBMonitor>(
        m_database.get(), m_accessController.get(), m_eventLogger.get());
    m_usbMonitorSignal.store(m_usbMonitor.get(), std::memory_order_release);
    m_initialized = true;
    return true;
}

bool ApplicationRuntime::Run() {
    if (!Initialize()) {
        return false;
    }

    if (m_stopRequested.load()) {
        return true;
    }

    return m_usbMonitor->Start();
}

void ApplicationRuntime::Stop() {
    m_stopRequested.store(true);
    USBMonitor* monitor = m_usbMonitorSignal.load(std::memory_order_acquire);
    if (monitor) {
        monitor->Stop();
    }
}

void ApplicationRuntime::Shutdown() {
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    if (m_shutdown) {
        return;
    }

    if (m_usbMonitor) {
        m_usbMonitor->Stop();
        m_usbMonitorSignal.store(nullptr, std::memory_order_release);
        m_usbMonitor.reset();
    }
    if (m_clientManager) {
        m_clientManager->Stop();
        m_clientManager.reset();
    }

    m_accessController.reset();
    m_serverClient.reset();
    m_eventLogger.reset();
    m_database.reset();
    m_initialized = false;
    m_shutdown = true;
}
