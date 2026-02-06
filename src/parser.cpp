#include "parser.h"
#include <fstream>
#include <shlobj.h>
#include <ctime>
#include <iomanip>
#include <sstream>

std::wstring UsageParser::ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(), nullptr, 0);
    if (len == 0) return L"";
    std::wstring w(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(), &w[0], len);
    return w;
}

std::string UsageParser::GetJsonValue(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos++;

    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) pos++;
    if (pos >= json.length()) return "";

    if (json[pos] == '"') {
        size_t end = json.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return json.substr(pos + 1, end - pos - 1);
    }

    size_t end = pos;
    while (end < json.length() && json[end] != ',' && json[end] != '}' && json[end] != ']' && json[end] != '\n') {
        end++;
    }
    std::string val = json.substr(pos, end - pos);
    while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
    return val;
}

int UsageParser::GetJsonInt(const std::string& json, const std::string& key) {
    std::string val = GetJsonValue(json, key);
    if (val.empty() || val == "null") return 0;
    try {
        return std::stoi(val);
    } catch (...) {
        return 0;
    }
}

float UsageParser::GetJsonFloat(const std::string& json, const std::string& key) {
    std::string val = GetJsonValue(json, key);
    if (val.empty() || val == "null") return 0.0f;
    try {
        return std::stof(val);
    } catch (...) {
        return 0.0f;
    }
}

std::string UsageParser::GetNestedBlock(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos++;

    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) pos++;
    if (pos >= json.length()) return "";

    if (json[pos] == '{') {
        int depth = 1;
        size_t start = pos;
        pos++;
        while (pos < json.length() && depth > 0) {
            if (json[pos] == '{') depth++;
            else if (json[pos] == '}') depth--;
            pos++;
        }
        return json.substr(start, pos - start);
    }
    return "";
}

std::wstring UsageParser::FormatResetTime(const std::string& isoTime) {
    if (isoTime.empty()) return L"";

    // Parse ISO 8601: "2026-02-04T21:00:00.490897+00:00"
    int year, month, day, hour, min, sec;
    if (sscanf_s(isoTime.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &min, &sec) < 6) {
        return L"";
    }

    // Convert to time_t (UTC)
    tm utcTm = {};
    utcTm.tm_year = year - 1900;
    utcTm.tm_mon = month - 1;
    utcTm.tm_mday = day;
    utcTm.tm_hour = hour;
    utcTm.tm_min = min;
    utcTm.tm_sec = sec;
    time_t resetTime = _mkgmtime(&utcTm);

    // Get current time
    time_t now = time(nullptr);
    double diffSec = difftime(resetTime, now);

    if (diffSec <= 0) {
        return L"Resetting...";
    }

    int totalMin = (int)(diffSec / 60);
    int hours = totalMin / 60;
    int mins = totalMin % 60;

    if (hours >= 24) {
        int days = hours / 24;
        hours = hours % 24;
        wchar_t buf[64];
        swprintf_s(buf, L"Resets in %dd %dh", days, hours);
        return buf;
    } else if (hours > 0) {
        wchar_t buf[64];
        swprintf_s(buf, L"Resets in %dh %dm", hours, mins);
        return buf;
    } else {
        wchar_t buf[64];
        swprintf_s(buf, L"Resets in %dm", mins);
        return buf;
    }
}

void UsageParser::SaveDebugDump(const std::string& body) {
    wchar_t appData[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData))) return;

    std::wstring dir = std::wstring(appData) + L"\\ClaudeUsageWidget";
    CreateDirectoryW(dir.c_str(), NULL);

    std::wstring path = dir + L"\\debug_response.txt";
    std::ofstream f(path, std::ios::binary);
    if (f) {
        f.write(body.c_str(), body.size());
    }
}

UsageData UsageParser::Parse(const std::string& body) {
    UsageData data;

    SaveDebugDump(body);

    if (body.empty()) {
        data.error = L"Empty response";
        return data;
    }

    // Parse Claude.ai usage format:
    // {"five_hour":{"utilization":93.0,"resets_at":"..."},"seven_day":{"utilization":55.0,"resets_at":"..."}}

    // Five hour (session) usage
    std::string fiveHour = GetNestedBlock(body, "five_hour");
    if (!fiveHour.empty()) {
        data.sessionPercent = GetJsonFloat(fiveHour, "utilization");
        std::string resetAt = GetJsonValue(fiveHour, "resets_at");
        data.sessionResetText = FormatResetTime(resetAt);
        data.sessionLimit = 100; // Percentage-based
        data.sessionUsed = (int)data.sessionPercent;
    }

    // Seven day (weekly) usage
    std::string sevenDay = GetNestedBlock(body, "seven_day");
    if (!sevenDay.empty()) {
        data.periodPercent = GetJsonFloat(sevenDay, "utilization");
        std::string resetAt = GetJsonValue(sevenDay, "resets_at");
        data.periodResetText = FormatResetTime(resetAt);
        data.periodLimit = 100; // Percentage-based
        data.periodUsed = (int)data.periodPercent;
    }

    data.periodLabel = L"Weekly";

    // Use the shorter reset time for display
    if (!data.sessionResetText.empty()) {
        data.resetText = data.sessionResetText;
    } else if (!data.periodResetText.empty()) {
        data.resetText = data.periodResetText;
    }

    // Valid if we got any data
    if (data.sessionPercent > 0 || data.periodPercent > 0 || !fiveHour.empty() || !sevenDay.empty()) {
        data.valid = true;
    } else {
        data.error = L"Could not parse usage data";
    }

    return data;
}
