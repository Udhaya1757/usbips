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

class AccessController {
public:
    explicit AccessController(LocalDatabase* db);

    DecisionResult EvaluateDevice(USBDevice& device);
    DecisionResult HandleUserDecision(USBDevice& device, bool userApproved, const std::string& friendlyName = "");

    void SetDatabase(LocalDatabase* db) { m_db = db; }
    LocalDatabase* GetDatabase() const { return m_db; }

private:
    LocalDatabase* m_db;
};
