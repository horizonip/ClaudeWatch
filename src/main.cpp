#include <windows.h>
#include <objidl.h>
#include <shellapi.h>
#include <commctrl.h>
#include <windowsx.h>
#include <string>
#include <ctime>

#include "resource.h"
#include "config.h"
#include "http_client.h"
#include "parser.h"
#include "ui.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

// Globals
static HWND g_hwnd = nullptr;
static WidgetUI g_ui;
static HttpClient g_http;
static UsageParser g_parser;
static UsageData g_usageData;
static bool g_offline = false;
static bool g_demoMode = false;
static wchar_t g_lastUpdate[64] = L"";
static UINT_PTR g_timerId = 0;
static bool g_dragging = false;
static POINT g_dragStart = { 0, 0 };

// Timer IDs
constexpr UINT_PTR TIMER_REFRESH = 1;
constexpr UINT TIMER_INTERVAL_MS = 60000; // Base: 1 minute

// Forward declarations
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RefreshUsage();
void ShowContextMenu(HWND hwnd, int x, int y);
void ShowCookieDialog(HWND hwnd);
int GetRefreshInterval();

// URLs for Claude.ai API
const wchar_t* ORGS_URL = L"https://claude.ai/api/organizations";
static std::wstring g_orgId;
static std::wstring g_usageUrl;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR cmdLine, int) {
    // Check for demo mode
    if (cmdLine && wcsstr(cmdLine, L"--demo")) {
        g_demoMode = true;
    }

    // Init common controls
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // Load config
    GetConfig().Load();

    // Init UI
    if (!g_ui.Init()) {
        MessageBoxW(nullptr, L"Failed to initialize GDI+", L"Error", MB_ICONERROR);
        return 1;
    }

    // Register window class
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ClaudeUsageWidgetClass";
    RegisterClassExW(&wc);

    // Get saved position or default
    Config& cfg = GetConfig().Get();
    int x = cfg.posX;
    int y = cfg.posY;

    // Ensure on screen (use virtual screen to support multi-monitor)
    int virtLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int virtTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int virtRight = virtLeft + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int virtBottom = virtTop + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (x < virtLeft) x = virtLeft;
    if (y < virtTop) y = virtTop;
    if (x > virtRight - WIDGET_WIDTH) x = virtRight - WIDGET_WIDTH;
    if (y > virtBottom - WIDGET_HEIGHT) y = virtBottom - WIDGET_HEIGHT;

    // Create window
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_LAYERED;
    if (cfg.alwaysOnTop) {
        exStyle |= WS_EX_TOPMOST;
    }

    g_hwnd = CreateWindowExW(
        exStyle,
        L"ClaudeUsageWidgetClass",
        L"Claude Usage",
        WS_POPUP,
        x, y, WIDGET_WIDTH, WIDGET_HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hwnd) {
        MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_ICONERROR);
        return 1;
    }

    // Set opacity
    SetLayeredWindowAttributes(g_hwnd, 0, (BYTE)(cfg.opacity * 255 / 100), LWA_ALPHA);

    // Show window
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    // Initial data
    if (g_demoMode) {
        g_usageData = UsageData::TestData();
        wcscpy_s(g_lastUpdate, L"Demo mode");
    } else if (!cfg.sessionCookie.empty()) {
        RefreshUsage();
    }

    // Start refresh timer
    g_timerId = SetTimer(g_hwnd, TIMER_REFRESH, GetRefreshInterval(), nullptr);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    KillTimer(g_hwnd, TIMER_REFRESH);
    g_ui.Shutdown();

    return (int)msg.wParam;
}

int GetRefreshInterval() {
    if (g_demoMode) return 60000; // 1 min in demo

    Config& cfg = GetConfig().Get();
    if (!cfg.smartRefresh) {
        return cfg.maxIntervalSec * 1000;
    }

    float usage = g_usageData.MaxPercent();
    if (usage >= 80.0f) return cfg.minIntervalSec * 1000;  // 1 min
    if (usage >= 50.0f) return 5 * 60 * 1000;              // 5 min
    return cfg.maxIntervalSec * 1000;                       // 10 min
}

std::string ExtractOrgId(const std::string& json) {
    // Look for "uuid" field in organizations response
    // Format: [{"uuid":"xxxx-xxxx-xxxx",...}]
    size_t pos = json.find("\"uuid\"");
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";

    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";

    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";

    return json.substr(pos + 1, end - pos - 1);
}

std::wstring ToWideString(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(), nullptr, 0);
    if (len == 0) return L"";
    std::wstring w(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(), &w[0], len);
    return w;
}

void RefreshUsage() {
    if (g_demoMode) return;

    Config& cfg = GetConfig().Get();
    if (cfg.sessionCookie.empty()) {
        g_usageData.valid = false;
        g_usageData.error = L"";
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return;
    }

    // Step 1: Get organization ID if we don't have it
    if (g_orgId.empty()) {
        HttpResponse orgResp = g_http.Get(ORGS_URL, cfg.sessionCookie);
        if (orgResp.status == HttpStatus::Success) {
            std::string orgId = ExtractOrgId(orgResp.body);
            if (!orgId.empty()) {
                g_orgId = ToWideString(orgId);
                g_usageUrl = L"https://claude.ai/api/organizations/" + g_orgId + L"/usage";
            }
        } else if (orgResp.status == HttpStatus::AuthError) {
            g_usageData.valid = false;
            g_usageData.error = L"Auth expired - update cookie";
            g_offline = false;
            InvalidateRect(g_hwnd, nullptr, FALSE);
            return;
        } else {
            g_offline = true;
            wcscpy_s(g_lastUpdate, L"Offline");
            InvalidateRect(g_hwnd, nullptr, FALSE);
            return;
        }
    }

    if (g_usageUrl.empty()) {
        g_usageData.valid = false;
        g_usageData.error = L"Could not get org ID";
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return;
    }

    // Step 2: Fetch usage data
    HttpResponse resp = g_http.Get(g_usageUrl, cfg.sessionCookie);

    if (resp.status == HttpStatus::Success) {
        g_usageData = g_parser.Parse(resp.body);
        g_offline = false;

        // Update timestamp
        time_t now = time(nullptr);
        tm local;
        localtime_s(&local, &now);
        swprintf_s(g_lastUpdate, L"Updated %02d:%02d", local.tm_hour, local.tm_min);
    } else if (resp.status == HttpStatus::AuthError) {
        g_usageData.valid = false;
        g_usageData.error = L"Auth expired - update cookie";
        g_offline = false;
        g_orgId.clear();
        g_usageUrl.clear();
    } else {
        // Network error - keep old data, mark offline
        g_offline = true;
        wcscpy_s(g_lastUpdate, L"Offline");
    }

    InvalidateRect(g_hwnd, nullptr, FALSE);
    UpdateWindow(g_hwnd);

    // Adjust timer based on new usage
    if (g_timerId) {
        KillTimer(g_hwnd, TIMER_REFRESH);
        g_timerId = SetTimer(g_hwnd, TIMER_REFRESH, GetRefreshInterval(), nullptr);
    }
}

void ShowContextMenu(HWND hwnd, int x, int y) {
    HMENU hMenu = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING, ID_MENU_REFRESH, L"Refresh Now");
    AppendMenuW(hMenu, MF_STRING, ID_MENU_SETCOOKIE, L"Set Cookie...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    Config& cfg = GetConfig().Get();
    AppendMenuW(hMenu, MF_STRING | (cfg.alwaysOnTop ? MF_CHECKED : 0), ID_MENU_ONTOP, L"Always On Top");
    AppendMenuW(hMenu, MF_STRING, ID_MENU_OPENSITE, L"Open Claude.ai");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_MENU_EXIT, L"Exit");

    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, x, y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
}

void ShowCookieDialog(HWND hwnd) {
    // Simple input dialog using MessageBox + clipboard approach
    // For a real implementation, create a proper dialog

    Config& cfg = GetConfig().Get();

    std::wstring msg = L"To set your session cookie:\n\n"
        L"1. Open Claude.ai in your browser\n"
        L"2. Press F12 to open DevTools\n"
        L"3. Go to Application > Cookies > claude.ai\n"
        L"4. Copy the 'sessionKey' value\n"
        L"5. Click OK below\n\n"
        L"Current cookie is ";
    msg += cfg.sessionCookie.empty() ? L"not set" : L"set";
    msg += L".\n\nThe cookie will be read from your clipboard automatically.";

    int result = MessageBoxW(hwnd, msg.c_str(), L"Set Cookie", MB_OKCANCEL | MB_ICONINFORMATION);

    if (result == IDOK) {
        // Get from clipboard
        if (OpenClipboard(hwnd)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
                if (pszText) {
                    cfg.sessionCookie = pszText;
                    // Trim whitespace
                    while (!cfg.sessionCookie.empty() &&
                           (cfg.sessionCookie.back() == L' ' ||
                            cfg.sessionCookie.back() == L'\n' ||
                            cfg.sessionCookie.back() == L'\r')) {
                        cfg.sessionCookie.pop_back();
                    }
                    GlobalUnlock(hData);

                    g_orgId.clear();
                    g_usageUrl.clear();
                    g_offline = false;
                    GetConfig().Save();
                    RefreshUsage();
                }
            }
            CloseClipboard();
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Double buffer
        RECT rc;
        GetClientRect(hwnd, &rc);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        g_ui.Render(memDC, rc.right, rc.bottom, g_usageData, g_offline, g_lastUpdate);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TIMER:
        if (wParam == TIMER_REFRESH) {
            RefreshUsage();
        }
        return 0;

    case WM_LBUTTONDOWN:
        g_dragging = true;
        g_dragStart.x = GET_X_LPARAM(lParam);
        g_dragStart.y = GET_Y_LPARAM(lParam);
        SetCapture(hwnd);
        return 0;

    case WM_MOUSEMOVE:
        if (g_dragging) {
            POINT pt;
            GetCursorPos(&pt);
            int newX = pt.x - g_dragStart.x;
            int newY = pt.y - g_dragStart.y;

            // Get the monitor the widget center would be on
            POINT centerPt = { newX + WIDGET_WIDTH / 2, newY + WIDGET_HEIGHT / 2 };
            HMONITOR hMon = MonitorFromPoint(centerPt, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfo(hMon, &mi);

            // Snap to edges of the current monitor's work area
            int monLeft = mi.rcWork.left;
            int monTop = mi.rcWork.top;
            int monRight = mi.rcWork.right;
            int monBottom = mi.rcWork.bottom;
            int snapDist = 15;

            if (newX < monLeft + snapDist) newX = monLeft;
            if (newY < monTop + snapDist) newY = monTop;
            if (newX > monRight - WIDGET_WIDTH - snapDist) newX = monRight - WIDGET_WIDTH;
            if (newY > monBottom - WIDGET_HEIGHT - snapDist) newY = monBottom - WIDGET_HEIGHT;

            SetWindowPos(hwnd, nullptr, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_dragging) {
            g_dragging = false;
            ReleaseCapture();

            // Save position
            RECT rc;
            GetWindowRect(hwnd, &rc);
            Config& cfg = GetConfig().Get();
            cfg.posX = rc.left;
            cfg.posY = rc.top;
            GetConfig().Save();
        }
        return 0;

    case WM_LBUTTONDBLCLK:
        ShellExecuteW(nullptr, L"open", L"https://claude.ai", nullptr, nullptr, SW_SHOW);
        return 0;

    case WM_RBUTTONUP:
        {
            POINT pt;
            GetCursorPos(&pt);
            ShowContextMenu(hwnd, pt.x, pt.y);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_MENU_REFRESH:
            RefreshUsage();
            break;

        case ID_MENU_SETCOOKIE:
            ShowCookieDialog(hwnd);
            break;

        case ID_MENU_ONTOP: {
            Config& cfg = GetConfig().Get();
            cfg.alwaysOnTop = !cfg.alwaysOnTop;
            SetWindowPos(hwnd, cfg.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                         0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            GetConfig().Save();
            break;
        }

        case ID_MENU_OPENSITE:
            ShellExecuteW(nullptr, L"open", L"https://claude.ai", nullptr, nullptr, SW_SHOW);
            break;

        case ID_MENU_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
