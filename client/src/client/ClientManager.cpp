#include "ClientManager.h"

#include "../network/ServerClient.h"

#include <iostream>

ClientManager::ClientManager(ServerClient* serverClient,
                             std::chrono::seconds heartbeatInterval)
    : m_serverClient(serverClient),
      m_heartbeatInterval(heartbeatInterval) {
    if (m_heartbeatInterval.count() <= 0) {
        m_heartbeatInterval = std::chrono::seconds(60);
    }
}

ClientManager::~ClientManager() {
    Stop();
}

bool ClientManager::Start() {
    if (!m_serverClient || m_serverClient->GetClientId().empty()) {
        return false;
    }

    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        return false;
    }

    m_thread = std::thread(&ClientManager::HeartbeatLoop, this);
    return true;
}

void ClientManager::Stop() {
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        return;
    }

    m_wakeup.notify_one();
    if (m_thread.joinable()) {
        m_thread.join();
    }

    if (m_serverClient && m_serverClient->SendHeartbeat("OFFLINE")) {
        std::wcout << L"[ClientManager] Final OFFLINE heartbeat sent.\n";
    } else {
        std::wcout << L"[ClientManager] Server unavailable; final OFFLINE heartbeat was not sent.\n";
    }
}

void ClientManager::HeartbeatLoop() {
    while (m_running.load()) {
        std::unique_lock<std::mutex> lock(m_wakeupMutex);
        if (m_wakeup.wait_for(lock, m_heartbeatInterval, [this] {
                return !m_running.load();
            })) {
            break;
        }
        lock.unlock();

        if (m_serverClient->SendHeartbeat("ONLINE")) {
            std::wcout << L"[ClientManager] Heartbeat sent.\n";
        } else {
            std::wcout << L"[ClientManager] Server unavailable; heartbeat will retry.\n";
        }
    }
}
