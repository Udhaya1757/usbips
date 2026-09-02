#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

class AccessController;
class ClientManager;
class EventLogger;
class LocalDatabase;
class ServerClient;
class USBMonitor;

class ApplicationRuntime {
public:
    explicit ApplicationRuntime(bool serviceMode = false);
    ~ApplicationRuntime();

    ApplicationRuntime(const ApplicationRuntime&) = delete;
    ApplicationRuntime& operator=(const ApplicationRuntime&) = delete;

    bool Initialize();
    bool Run();
    void Stop();
    void Shutdown();

    bool IsServiceMode() const { return m_serviceMode; }

private:
    static std::chrono::seconds GetHeartbeatInterval();
    bool InitializeServiceDataDirectory();
    std::string GetDataPath(const std::string& fileName) const;

    bool m_serviceMode;
    std::wstring m_serviceDataDirectory;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<USBMonitor*> m_usbMonitorSignal{nullptr};
    std::mutex m_lifecycleMutex;
    bool m_initialized = false;
    bool m_shutdown = false;

    std::unique_ptr<LocalDatabase> m_database;
    std::unique_ptr<EventLogger> m_eventLogger;
    std::unique_ptr<ServerClient> m_serverClient;
    std::unique_ptr<AccessController> m_accessController;
    std::unique_ptr<ClientManager> m_clientManager;
    std::unique_ptr<USBMonitor> m_usbMonitor;
};
