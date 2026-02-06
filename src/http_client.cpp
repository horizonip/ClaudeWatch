#include "http_client.h"
#include <windows.h>
#include <winhttp.h>
#include <vector>

#pragma comment(lib, "winhttp.lib")

HttpClient::HttpClient() {
    m_session = WinHttpOpen(
        L"ClaudeUsageWidget/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
}

HttpClient::~HttpClient() {
    if (m_session) {
        WinHttpCloseHandle(m_session);
    }
}

HttpResponse HttpClient::Get(const std::wstring& url, const std::wstring& cookie) {
    HttpResponse response;
    response.status = HttpStatus::NetworkError;
    response.statusCode = 0;

    if (!m_session) {
        response.error = L"Failed to initialize WinHTTP";
        return response;
    }

    // Parse URL
    URL_COMPONENTS urlComp = { 0 };
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256] = { 0 };
    wchar_t urlPath[2048] = { 0 };

    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = _countof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = _countof(urlPath);

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp)) {
        response.error = L"Failed to parse URL";
        return response;
    }

    // Connect
    HINTERNET hConnect = WinHttpConnect(
        m_session,
        hostName,
        urlComp.nPort,
        0
    );

    if (!hConnect) {
        response.error = L"Failed to connect";
        return response;
    }

    // Create request
    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        urlPath,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        response.error = L"Failed to create request";
        return response;
    }

    // Add cookie header
    if (!cookie.empty()) {
        std::wstring cookieHeader = L"Cookie: sessionKey=" + cookie;
        WinHttpAddRequestHeaders(hRequest, cookieHeader.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        response.error = L"Failed to send request";
        return response;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        response.error = L"Failed to receive response";
        return response;
    }

    // Get status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX
    );
    response.statusCode = statusCode;

    // Check status
    if (statusCode == 401 || statusCode == 403) {
        response.status = HttpStatus::AuthError;
        response.error = L"Authentication failed - cookie may be expired";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return response;
    }

    if (statusCode >= 500) {
        response.status = HttpStatus::ServerError;
        response.error = L"Server error";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return response;
    }

    // Read body
    std::vector<char> buffer;
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;

    do {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
            break;
        }

        if (bytesAvailable == 0) {
            break;
        }

        std::vector<char> chunk(bytesAvailable + 1);
        if (WinHttpReadData(hRequest, chunk.data(), bytesAvailable, &bytesRead)) {
            buffer.insert(buffer.end(), chunk.begin(), chunk.begin() + bytesRead);
        }
    } while (bytesAvailable > 0);

    response.body = std::string(buffer.begin(), buffer.end());
    response.status = HttpStatus::Success;

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);

    return response;
}
