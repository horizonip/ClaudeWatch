#pragma once

#include <string>
#include <functional>

enum class HttpStatus {
    Success,
    NetworkError,
    AuthError,      // 401, 403
    ServerError,    // 5xx
    ParseError
};

struct HttpResponse {
    HttpStatus status;
    int statusCode;
    std::string body;
    std::wstring error;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpResponse Get(const std::wstring& url, const std::wstring& cookie);

private:
    void* m_session; // HINTERNET
};
