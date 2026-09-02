#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "../device/USBDevice.h"
#include "../database/LocalDatabase.h"
#include <string>

enum class AccessDecision {
    ALLOW,
    BLOCK,
    ASK
};

inline std::wstring AccessDecisionToWString(AccessDecision decision) {
    switch (decision) {
        case AccessDecision::ALLOW: return L"ALLOW";
        case AccessDecision::BLOCK: return L"BLOCK";
        case AccessDecision::ASK:   return L"ASK";
        default:                    return L"UNKNOWN";
    }
}

inline std::string AccessDecisionToString(AccessDecision decision) {
    switch (decision) {
        case AccessDecision::ALLOW: return "ALLOW";
        case AccessDecision::BLOCK: return "BLOCK";
        case AccessDecision::ASK:   return "ASK";
        default:                    return "UNKNOWN";
    }
}

struct DecisionResult {
    AccessDecision decision;
    std::string reason;
};

class ServerClient;

class AccessController {
public:
    explicit AccessController(LocalDatabase* db, ServerClient* serverClient = nullptr);

    DecisionResult EvaluateDevice(USBDevice& device);
    DecisionResult HandleUserDecision(USBDevice& device, bool userApproved, const std::string& friendlyName = "");
    void SetInteractivePrompts(bool enabled) { m_interactivePrompts = enabled; }
    bool IsInteractivePromptsEnabled() const { return m_interactivePrompts; }

    void SetDatabase(LocalDatabase* db) { m_db = db; }
    LocalDatabase* GetDatabase() const { return m_db; }

    void SetServerClient(ServerClient* serverClient) { m_serverClient = serverClient; }
    ServerClient* GetServerClient() const { return m_serverClient; }

private:
    LocalDatabase* m_db;
    ServerClient* m_serverClient;
    bool m_interactivePrompts = true;
};
