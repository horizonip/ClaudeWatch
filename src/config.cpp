#include "config.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <wincrypt.h>
#include <vector>

#pragma comment(lib, "crypt32.lib")

static ConfigManager g_configManager;

ConfigManager& GetConfig() {
    return g_configManager;
}

ConfigManager::ConfigManager() {
    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        m_configDir = std::wstring(appDataPath) + L"\\ClaudeUsageWidget";
        m_configPath = m_configDir + L"\\config.ini";
    }
}

bool ConfigManager::EnsureConfigDir() {
    if (m_configDir.empty()) return false;

    DWORD attrs = GetFileAttributesW(m_configDir.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return CreateDirectoryW(m_configDir.c_str(), NULL) != 0;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring ConfigManager::ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* def) {
    wchar_t buffer[4096];
    GetPrivateProfileStringW(section, key, def, buffer, _countof(buffer), m_configPath.c_str());
    return buffer;
}

int ConfigManager::ReadInt(const wchar_t* section, const wchar_t* key, int def) {
    return GetPrivateProfileIntW(section, key, def, m_configPath.c_str());
}

void ConfigManager::WriteString(const wchar_t* section, const wchar_t* key, const std::wstring& value) {
    WritePrivateProfileStringW(section, key, value.c_str(), m_configPath.c_str());
}

void ConfigManager::WriteInt(const wchar_t* section, const wchar_t* key, int value) {
    WriteString(section, key, std::to_wstring(value));
}

std::wstring ConfigManager::EncryptString(const std::wstring& plaintext) {
    if (plaintext.empty()) return L"";

    // Convert wstring to UTF-8 bytes for encryption
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, plaintext.c_str(), (int)plaintext.length(), nullptr, 0, nullptr, nullptr);
    if (utf8Len == 0) return L"";
    std::vector<BYTE> utf8(utf8Len);
    WideCharToMultiByte(CP_UTF8, 0, plaintext.c_str(), (int)plaintext.length(), (char*)utf8.data(), utf8Len, nullptr, nullptr);

    DATA_BLOB input = { (DWORD)utf8.size(), utf8.data() };
    DATA_BLOB output = {};

    if (!CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        return L"";
    }

    // Encode as hex string for INI storage
    std::wstring hex;
    hex.reserve(output.cbData * 2);
    for (DWORD i = 0; i < output.cbData; i++) {
        wchar_t buf[3];
        swprintf_s(buf, L"%02X", output.pbData[i]);
        hex += buf;
    }
    LocalFree(output.pbData);
    return hex;
}

std::wstring ConfigManager::DecryptString(const std::wstring& hexCiphertext) {
    if (hexCiphertext.empty()) return L"";

    // Decode hex to bytes
    if (hexCiphertext.length() % 2 != 0) return L"";
    std::vector<BYTE> encrypted(hexCiphertext.length() / 2);
    for (size_t i = 0; i < encrypted.size(); i++) {
        unsigned int byte;
        if (swscanf_s(hexCiphertext.c_str() + i * 2, L"%02X", &byte) != 1) return L"";
        encrypted[i] = (BYTE)byte;
    }

    DATA_BLOB input = { (DWORD)encrypted.size(), encrypted.data() };
    DATA_BLOB output = {};

    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        return L"";
    }

    // Convert UTF-8 bytes back to wstring
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, (char*)output.pbData, output.cbData, nullptr, 0);
    std::wstring result(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, (char*)output.pbData, output.cbData, &result[0], wideLen);
    LocalFree(output.pbData);
    return result;
}

bool ConfigManager::Load() {
    if (m_configPath.empty()) return false;

    // Check if config file exists
    if (GetFileAttributesW(m_configPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    // Auth - try encrypted first, fall back to plaintext for migration
    std::wstring encCookie = ReadString(L"Auth", L"SessionCookieEnc", L"");
    if (!encCookie.empty()) {
        m_config.sessionCookie = DecryptString(encCookie);
    } else {
        m_config.sessionCookie = ReadString(L"Auth", L"SessionCookie", L"");
    }

    // Window
    m_config.posX = ReadInt(L"Window", L"PosX", 100);
    m_config.posY = ReadInt(L"Window", L"PosY", 100);
    m_config.alwaysOnTop = ReadInt(L"Window", L"AlwaysOnTop", 1) != 0;
    m_config.opacity = ReadInt(L"Window", L"Opacity", 90);

    // Refresh
    m_config.smartRefresh = ReadInt(L"Refresh", L"SmartRefresh", 1) != 0;
    m_config.minIntervalSec = ReadInt(L"Refresh", L"MinIntervalSec", 60);
    m_config.maxIntervalSec = ReadInt(L"Refresh", L"MaxIntervalSec", 600);

    // Display
    m_config.showResetTime = ReadInt(L"Display", L"ShowResetTime", 1) != 0;

    return true;
}

bool ConfigManager::Save() {
    if (m_configPath.empty()) return false;
    if (!EnsureConfigDir()) return false;

    // Auth - always write encrypted, clear plaintext key
    WriteString(L"Auth", L"SessionCookieEnc", EncryptString(m_config.sessionCookie));
    WriteString(L"Auth", L"SessionCookie", L"");

    // Window
    WriteInt(L"Window", L"PosX", m_config.posX);
    WriteInt(L"Window", L"PosY", m_config.posY);
    WriteInt(L"Window", L"AlwaysOnTop", m_config.alwaysOnTop ? 1 : 0);
    WriteInt(L"Window", L"Opacity", m_config.opacity);

    // Refresh
    WriteInt(L"Refresh", L"SmartRefresh", m_config.smartRefresh ? 1 : 0);
    WriteInt(L"Refresh", L"MinIntervalSec", m_config.minIntervalSec);
    WriteInt(L"Refresh", L"MaxIntervalSec", m_config.maxIntervalSec);

    // Display
    WriteInt(L"Display", L"ShowResetTime", m_config.showResetTime ? 1 : 0);

    return true;
}
