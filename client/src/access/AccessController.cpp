#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "AccessController.h"
#include "../network/ServerClient.h"
#include <iostream>

AccessController::AccessController(LocalDatabase* db, ServerClient* serverClient)
    : m_db(db)
    , m_serverClient(serverClient)
{
}

DecisionResult AccessController::EvaluateDevice(USBDevice& device) {
    if (!m_db) {
        return { AccessDecision::ASK, "DATABASE_UNAVAILABLE" };
    }

    if (m_db->IsDeviceAllowed(device)) {
        m_db->LogEvent("EVALUATED", device, "ALLOW", "ALLOWLIST_MATCH");
        return { AccessDecision::ALLOW, "ALLOWLIST_MATCH" };
    }

    if (m_serverClient) {
        ServerClient::PolicyResult remote = m_serverClient->CheckDevicePolicy(
            device.vid,
            device.pid,
            device.serialNumber,
            device.deviceType
        );

        if (remote.success) {
            const std::string decision = remote.allowed ? "ALLOW" : "BLOCK";
            const std::string reason = remote.reason.empty() ? (remote.allowed ? "ALLOWLIST_MATCH" : "DEVICE_BLOCKED") : remote.reason;
            m_db->LogEvent("EVALUATED", device, decision, reason);
            return {
                remote.allowed ? AccessDecision::ALLOW : AccessDecision::BLOCK,
                reason
            };
        }

        m_db->LogEvent("EVALUATED", device, "ASK", "SERVER_UNAVAILABLE");
        return { AccessDecision::ASK, "SERVER_UNAVAILABLE" };
    }

    m_db->LogEvent("EVALUATED", device, "ASK", "UNKNOWN_DEVICE");
    return { AccessDecision::ASK, "UNKNOWN_DEVICE" };
}

DecisionResult AccessController::HandleUserDecision(USBDevice& device, bool userApproved, const std::string& friendlyName) {
    if (!m_db) {
        return { userApproved ? AccessDecision::ALLOW : AccessDecision::BLOCK, "DATABASE_UNAVAILABLE" };
    }

    if (userApproved) {
        m_db->AddDevice(device, friendlyName.empty() ? LocalDatabase::ToUtf8(device.description) : friendlyName);
        m_db->LogEvent("USER_DECISION", device, "ALLOW", "USER_APPROVED");
        return { AccessDecision::ALLOW, "USER_APPROVED" };
    }

    m_db->LogEvent("USER_DECISION", device, "BLOCK", "USER_REJECTED");
    return { AccessDecision::BLOCK, "USER_REJECTED" };
}
