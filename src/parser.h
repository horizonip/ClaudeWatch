#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

struct UsageData {
    bool valid = false;
    std::wstring error;

    // Session (5-hour) - percentage based
    float sessionPercent = 0.0f;
    int sessionUsed = 0;    // For display as "93%"
    int sessionLimit = 100;
    std::wstring sessionResetText;

    // Period (7-day) - percentage based
    float periodPercent = 0.0f;
    int periodUsed = 0;
    int periodLimit = 100;
    std::wstring periodResetText;
    std::wstring periodLabel = L"Weekly";

    // Combined reset text for footer
    std::wstring resetText;

    float SessionPercent() const { return sessionPercent; }
    float PeriodPercent() const { return periodPercent; }

    float MaxPercent() const {
        return sessionPercent > periodPercent ? sessionPercent : periodPercent;
    }

    static UsageData TestData() {
        UsageData d;
        d.valid = true;
        d.sessionPercent = 93.0f;
        d.sessionUsed = 93;
        d.sessionLimit = 100;
        d.sessionResetText = L"Resets in 4h 23m";
        d.periodPercent = 55.0f;
        d.periodUsed = 55;
        d.periodLimit = 100;
        d.periodResetText = L"Resets in 5d 2h";
        d.periodLabel = L"Weekly";
        d.resetText = L"Resets in 4h 23m";
        return d;
    }
};

class UsageParser {
public:
    UsageData Parse(const std::string& body);
    void SaveDebugDump(const std::string& body);

private:
    std::wstring ToWide(const std::string& s);
    std::string GetJsonValue(const std::string& json, const std::string& key);
    int GetJsonInt(const std::string& json, const std::string& key);
    float GetJsonFloat(const std::string& json, const std::string& key);
    std::string GetNestedBlock(const std::string& json, const std::string& key);
    std::wstring FormatResetTime(const std::string& isoTime);
};
