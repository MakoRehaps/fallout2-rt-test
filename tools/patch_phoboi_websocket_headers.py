from pathlib import Path

p = Path('src/local_coop_mobile.cc')
s = p.read_text(encoding='utf-8')
marker = '// PHOBOI_CASE_INSENSITIVE_WEBSOCKET_HEADERS_V1'
if marker in s:
    print('PhoBoi WebSocket header fix already applied')
    raise SystemExit(0)

old = '''std::string mobileHeaderValue(const std::string& request, const char* name)
{
    std::string prefix = std::string(name) + ":";
    size_t at = request.find(prefix);
    if (at == std::string::npos) {
        return "";
    }
    at += prefix.size();
    while (at < request.size() && (request[at] == ' ' || request[at] == '\\t')) {
        at++;
    }
    size_t end = request.find("\\r\\n", at);
    return request.substr(at, end == std::string::npos ? std::string::npos : end - at);
}
'''

new = '''// PHOBOI_CASE_INSENSITIVE_WEBSOCKET_HEADERS_V1
std::string mobileHeaderValue(const std::string& request, const char* name)
{
    // HTTP field names are case-insensitive. Reverse proxies such as Cloudflare
    // are allowed to normalize header casing, so never search the raw request
    // with a case-sensitive substring match.
    std::string wanted(name != nullptr ? name : "");
    std::transform(wanted.begin(), wanted.end(), wanted.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    size_t lineStart = 0;
    while (lineStart < request.size()) {
        size_t lineEnd = request.find("\\r\\n", lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = request.size();
        }
        size_t colon = request.find(':', lineStart);
        if (colon != std::string::npos && colon < lineEnd) {
            std::string field = request.substr(lineStart, colon - lineStart);
            std::transform(field.begin(), field.end(), field.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (field == wanted) {
                size_t valueStart = colon + 1;
                while (valueStart < lineEnd
                    && (request[valueStart] == ' ' || request[valueStart] == '\\t')) {
                    valueStart++;
                }
                return request.substr(valueStart, lineEnd - valueStart);
            }
        }
        if (lineEnd >= request.size()) {
            break;
        }
        lineStart = lineEnd + 2;
    }
    return "";
}
'''

if old not in s:
    raise SystemExit('Expected mobileHeaderValue implementation not found')

# std::tolower declaration.
if '#include <cctype>\n' not in s:
    s = s.replace('#include <chrono>\n', '#include <chrono>\n#include <cctype>\n', 1)

s = s.replace(old, new, 1)

# Add useful handshake diagnostics without exposing the session token.
s = s.replace(
'''    std::string key = mobileHeaderValue(request, "Sec-WebSocket-Key");
    if (key.empty()) {
        mobileSendResponse(socket, "400 Bad Request", "text/plain", "Missing WebSocket key");
        return;
    }
''',
'''    std::string key = mobileHeaderValue(request, "Sec-WebSocket-Key");
    if (key.empty()) {
        debugPrint("[PHOBOI WS] control handshake missing Sec-WebSocket-Key slot=%d\\n", slot + 1);
        mobileSendResponse(socket, "400 Bad Request", "text/plain", "Missing WebSocket key");
        return;
    }
    debugPrint("[PHOBOI WS] control handshake accepted slot=%d\\n", slot + 1);
''', 1)

s = s.replace(
'''    std::string key = mobileHeaderValue(request, "Sec-WebSocket-Key");
    if (key.empty()) return;
''',
'''    std::string key = mobileHeaderValue(request, "Sec-WebSocket-Key");
    if (key.empty()) {
        debugPrint("[PHOBOI WS] stream handshake missing Sec-WebSocket-Key slot=%d\\n", slot + 1);
        return;
    }
    debugPrint("[PHOBOI WS] stream handshake accepted slot=%d\\n", slot + 1);
''', 1)

p.write_text(s, encoding='utf-8')
print('Applied PhoBoi case-insensitive WebSocket header fix')
