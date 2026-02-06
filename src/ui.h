#pragma once

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "parser.h"

#pragma comment(lib, "gdiplus.lib")

class WidgetUI {
public:
    WidgetUI();
    ~WidgetUI();

    bool Init();
    void Shutdown();

    void Render(HDC hdc, int width, int height, const UsageData& data, bool offline, const wchar_t* lastUpdate);

    // Colors based on usage percentage
    static COLORREF GetBarColor(float percent);

private:
    ULONG_PTR m_gdiplusToken = 0;
    Gdiplus::Font* m_fontSmall = nullptr;
    Gdiplus::Font* m_fontNormal = nullptr;

    void DrawProgressBar(Gdiplus::Graphics& g, int x, int y, int w, int h,
                         float percent, const wchar_t* label, int used, int limit);
};

// Widget window dimensions
constexpr int WIDGET_WIDTH = 260;
constexpr int WIDGET_HEIGHT = 95;
