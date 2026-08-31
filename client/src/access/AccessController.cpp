#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "AccessController.h"
#include <iostream>

AccessController::AccessController(LocalDatabase* db)
    : m_db(db)
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
