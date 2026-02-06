# Claude Usage Widget - Design Document

## Overview

A tiny Windows desktop widget displaying Claude.ai plan usage limits (session and weekly) as visual progress bars. Built with pure Win32 API for minimal footprint.

## Requirements

- Show current session usage vs limit
- Show weekly usage vs limit
- Color-coded progress bars (green/yellow/red based on usage %)
- Smart auto-refresh (faster when usage is high)
- Desktop-docked gadget style
- Single executable under 100KB
- No external dependencies

## Architecture

```
┌─────────────────────────────────────────┐
│           Claude Usage Widget           │
├─────────────────────────────────────────┤
│  ┌─────────────────────────────────┐    │
│  │ Session    ████████░░  156/200  │    │
│  └─────────────────────────────────┘    │
│  ┌─────────────────────────────────┐    │
│  │ Weekly     ██████░░░░  892/1500 │    │
│  └─────────────────────────────────┘    │
│  ↻ 2m ago          Resets in 3d 4h     │
└─────────────────────────────────────────┘
```

### Core Modules

| Module | Responsibility |
|--------|----------------|
| `main.cpp` | WinMain, message loop, window creation |
| `http_client.cpp` | WinHTTP wrapper for fetching Claude.ai |
| `parser.cpp` | HTML/JSON parsing for usage data |
| `ui.cpp` | GDI+ rendering, progress bars |
| `config.cpp` | INI file read/write |

## Data Fetching

### Endpoint
- URL: `https://claude.ai/settings/usage` (or account page with usage data)
- Auth: `Cookie: sessionKey=sk-ant-...` header

### Request Flow
1. WinHTTP GET request with session cookie
2. Receive HTML response
3. Parse for usage JSON in `<script>` tags or HTML elements
4. Extract: session_usage, session_limit, weekly_usage, weekly_limit, reset_time

### Error Handling
- 401/403: Cookie expired → show "Auth needed" indicator
- Network error: Show last known values with "offline" indicator
- Parse error: Log to file, show "?" for affected values

### Smart Refresh Logic
| Usage Level | Refresh Interval |
|-------------|------------------|
| < 50% | 10 minutes |
| 50-80% | 5 minutes |
| > 80% | 1 minute |

## UI Specification

### Window Properties
- Style: `WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`
- Size: ~250x80 pixels
- Rendering: GDI+ double-buffered

### Color Scheme
| Usage % | Color |
|---------|-------|
| < 50% | Green |
| 50-80% | Yellow |
| > 80% | Red |

### Interactions
| Action | Behavior |
|--------|----------|
| Right-click | Context menu (Refresh, Settings, Copy Cookie, Exit) |
| Left-drag | Reposition widget (saves position) |
| Double-click | Open Claude.ai in browser |
| Hover on bar | Tooltip with exact numbers |

### Desktop Behavior
- Snaps to screen edges when dragged near
- Remembers position across restarts
- Stays visible on "Show Desktop"

## Configuration

### File Location
`%APPDATA%\ClaudeWatch\config.ini`

### Config Format
```ini
[Auth]
SessionCookie=sk-ant-sid01-xxxxx...

[Window]
PosX=1650
PosY=50
AlwaysOnTop=1
Opacity=90

[Refresh]
SmartRefresh=1
MinIntervalSec=60
MaxIntervalSec=600

[Display]
ShowResetTime=1
ColorScheme=auto
```

### First-Run Experience
1. Widget appears with "Click to configure" message
2. Right-click → "Set Cookie" opens dialog
3. Paste cookie from browser DevTools
4. Test and save

## Project Structure

```
ClaudeWatch/
├── src/
│   ├── main.cpp
│   ├── http_client.cpp
│   ├── parser.cpp
│   ├── ui.cpp
│   ├── config.cpp
│   └── resource.h
├── res/
│   ├── app.ico
│   └── app.rc
├── CMakeLists.txt
└── README.md
```

## Dependencies

All Windows built-in (no external libraries):
- `WinHTTP.lib` - HTTP requests
- `Gdiplus.lib` - Anti-aliased rendering
- `Shlwapi.lib` - Path utilities
- `Comctl32.lib` - Common controls

## Build

- Compiler: MSVC or MinGW
- Build system: CMake
- Optimization: `/O2 /LTCG` for size
- Target size: 60-80KB single .exe

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| Claude.ai page structure changes | Keep parser modular, easy to update patterns |
| Session cookie expires frequently | Clear "Auth needed" indicator, easy re-paste flow |
| Rate limiting from frequent requests | Smart refresh reduces unnecessary requests |
