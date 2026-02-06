#pragma once

#include <string>
#include <windows.h>

struct Config {
    // Auth
    std::wstring sessionCookie;

    // Window
    int posX = 100;
    int posY = 100;
    bool alwaysOnTop = true;
    int opacity = 90;

    // Refresh
    bool smartRefresh = true;
    int minIntervalSec = 60;
    int maxIntervalSec = 600;

    // Display
    bool showResetTime = true;
};

class ConfigManager {
public:
    ConfigManager();

    bool Load();
    bool Save();

    Config& Get() { return m_config; }
    const Config& Get() const { return m_config; }

    std::wstring GetConfigPath() const { return m_configPath; }

private:
    std::wstring m_configPath;
    std::wstring m_configDir;
    Config m_config;

    bool EnsureConfigDir();
    std::wstring ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* def);
    int ReadInt(const wchar_t* section, const wchar_t* key, int def);
    void WriteString(const wchar_t* section, const wchar_t* key, const std::wstring& value);
    void WriteInt(const wchar_t* section, const wchar_t* key, int value);

    std::wstring EncryptString(const std::wstring& plaintext);
    std::wstring DecryptString(const std::wstring& hexCiphertext);
};

ConfigManager& GetConfig();
