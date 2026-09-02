#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

class ServerClient;

class ClientManager {
public:
    explicit ClientManager(ServerClient* serverClient,
                           std::chrono::seconds heartbeatInterval = std::chrono::seconds(60));
    ~ClientManager();

    ClientManager(const ClientManager&) = delete;
    ClientManager& operator=(const ClientManager&) = delete;

    bool Start();
    void Stop();

private:
    void HeartbeatLoop();

    ServerClient* m_serverClient;
    std::chrono::seconds m_heartbeatInterval;
    std::atomic<bool> m_running{false};
    std::condition_variable m_wakeup;
    std::mutex m_wakeupMutex;
    std::thread m_thread;
};
