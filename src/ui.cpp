#include "ui.h"
#include <string>

using namespace Gdiplus;

WidgetUI::WidgetUI() {}

WidgetUI::~WidgetUI() {
    Shutdown();
}

bool WidgetUI::Init() {
    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr) != Ok) {
        return false;
    }

    m_fontSmall = new Font(L"Segoe UI", 9);
    m_fontNormal = new Font(L"Segoe UI", 10);

    return true;
}

void WidgetUI::Shutdown() {
    delete m_fontSmall;
    delete m_fontNormal;
    m_fontSmall = nullptr;
    m_fontNormal = nullptr;

    if (m_gdiplusToken) {
        GdiplusShutdown(m_gdiplusToken);
        m_gdiplusToken = 0;
    }
}

COLORREF WidgetUI::GetBarColor(float percent) {
    if (percent >= 80.0f) return RGB(220, 53, 69);   // Red
    if (percent >= 50.0f) return RGB(255, 193, 7);   // Yellow
    return RGB(40, 167, 69);                          // Green
}

void WidgetUI::DrawProgressBar(Graphics& g, int x, int y, int w, int h,
                                float percent, const wchar_t* label, int used, int limit) {
    // Background
    SolidBrush bgBrush(Color(40, 40, 45));
    g.FillRectangle(&bgBrush, x, y, w, h);

    // Progress fill
    int fillW = (int)(w * percent / 100.0f);
    if (fillW > w) fillW = w;

    COLORREF barColor = GetBarColor(percent);
    SolidBrush fillBrush(Color(GetRValue(barColor), GetGValue(barColor), GetBValue(barColor)));
    if (fillW > 0) {
        g.FillRectangle(&fillBrush, x, y, fillW, h);
    }

    // Border
    Pen borderPen(Color(60, 60, 65), 1);
    g.DrawRectangle(&borderPen, x, y, w, h);

    // Label on left
    SolidBrush textBrush(Color(220, 220, 220));
    StringFormat sf;
    sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentCenter);

    RectF labelRect((float)x + 6, (float)y, (float)w / 2, (float)h);
    g.DrawString(label, -1, m_fontSmall, labelRect, &sf, &textBrush);

    // Percentage on right
    wchar_t numBuf[32];
    if (percent > 0 || limit > 0) {
        swprintf_s(numBuf, L"%.0f%%", percent);
    } else {
        swprintf_s(numBuf, L"--");
    }

    sf.SetAlignment(StringAlignmentFar);
    RectF numRect((float)x + w / 2, (float)y, (float)w / 2 - 6, (float)h);
    g.DrawString(numBuf, -1, m_fontSmall, numRect, &sf, &textBrush);
}

void WidgetUI::Render(HDC hdc, int width, int height, const UsageData& data, bool offline, const wchar_t* lastUpdate) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Background
    SolidBrush bgBrush(Color(30, 30, 35));
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    // Border
    Pen borderPen(Color(50, 50, 55), 1);
    g.DrawRectangle(&borderPen, 0, 0, width - 1, height - 1);

    int margin = 10;
    int barHeight = 22;
    int barWidth = width - margin * 2;

    if (!data.valid && !data.error.empty()) {
        // Error state
        SolidBrush errBrush(Color(220, 53, 69));
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        RectF errRect(0, 0, (float)width, (float)height);
        g.DrawString(data.error.c_str(), -1, m_fontNormal, errRect, &sf, &errBrush);
        return;
    }

    if (!data.valid) {
        // Not configured
        SolidBrush textBrush(Color(150, 150, 150));
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        RectF rect(0, 0, (float)width, (float)height);
        g.DrawString(L"Right-click to configure", -1, m_fontNormal, rect, &sf, &textBrush);
        return;
    }

    // Session bar (5-hour limit)
    DrawProgressBar(g, margin, margin, barWidth, barHeight,
                    data.SessionPercent(), L"5 Hour", data.sessionUsed, data.sessionLimit);

    // Period bar
    std::wstring periodLabel = data.periodLabel;
    DrawProgressBar(g, margin, margin + barHeight + 6, barWidth, barHeight,
                    data.PeriodPercent(), periodLabel.c_str(), data.periodUsed, data.periodLimit);

    // Footer text
    SolidBrush footerBrush(Color(120, 120, 125));
    StringFormat sfLeft, sfRight;
    sfLeft.SetAlignment(StringAlignmentNear);
    sfRight.SetAlignment(StringAlignmentFar);

    int footerY = margin + (barHeight + 6) * 2 + 4;

    // Last update on left
    std::wstring updateText = offline ? L"Offline" : (lastUpdate ? lastUpdate : L"");
    RectF leftRect((float)margin, (float)footerY, (float)barWidth / 2, 16);
    g.DrawString(updateText.c_str(), -1, m_fontSmall, leftRect, &sfLeft, &footerBrush);

    // Reset time on right
    if (!data.resetText.empty()) {
        RectF rightRect((float)width / 2, (float)footerY, (float)barWidth / 2, 16);
        g.DrawString(data.resetText.c_str(), -1, m_fontSmall, rightRect, &sfRight, &footerBrush);
    }

    // Offline indicator
    if (offline) {
        Pen offlinePen(Color(220, 53, 69), 2);
        g.DrawRectangle(&offlinePen, 1, 1, width - 3, height - 3);
    }
}
