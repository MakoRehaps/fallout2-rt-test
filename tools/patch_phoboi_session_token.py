#!/usr/bin/env python3
from pathlib import Path

p = Path('src/local_coop_mobile.cc')
s = p.read_text(encoding='utf-8')

MARKER = '// PHOBOI_UNSIGNED_SESSION_TOKEN_V1'
if MARKER in s:
    print('PhoBoi unsigned token fix already applied')
    raise SystemExit(0)

anchor = '''int mobileValue(const std::string& values, const char* key, int fallback)\n{\n    std::string prefix = std::string(key) + "=";\n    size_t start = 0;\n    while (start < values.size()) {\n        size_t end = values.find('&', start);\n        if (end == std::string::npos) {\n            end = values.size();\n        }\n\n        if (values.compare(start, prefix.size(), prefix) == 0) {\n            return std::atoi(values.substr(start + prefix.size(), end - start - prefix.size()).c_str());\n        }\n\n        start = end + 1;\n    }\n\n    return fallback;\n}\n'''

if anchor not in s:
    raise SystemExit('mobileValue anchor not found')

replacement = anchor + r'''

// PHOBOI_UNSIGNED_SESSION_TOKEN_V1
uint32_t mobileUnsignedValue(const std::string& values, const char* key, uint32_t fallback)
{
    std::string prefix = std::string(key) + "=";
    size_t start = 0;
    while (start < values.size()) {
        size_t end = values.find('&', start);
        if (end == std::string::npos) {
            end = values.size();
        }

        if (values.compare(start, prefix.size(), prefix) == 0) {
            std::string text = values.substr(start + prefix.size(), end - start - prefix.size());
            char* tail = nullptr;
            unsigned long long parsed = std::strtoull(text.c_str(), &tail, 10);
            if (tail == text.c_str() || *tail != '\0' || parsed > 0xFFFFFFFFull) {
                return fallback;
            }
            return static_cast<uint32_t>(parsed);
        }

        start = end + 1;
    }

    return fallback;
}
'''

s = s.replace(anchor, replacement, 1)
old = 'uint32_t token = static_cast<uint32_t>(mobileValue(values, "token", 0));'
count = s.count(old)
if count < 2:
    raise SystemExit(f'expected at least 2 token parse sites, found {count}')
s = s.replace(old, 'uint32_t token = mobileUnsignedValue(values, "token", 0);')

p.write_text(s, encoding='utf-8')
print(f'PhoBoi unsigned token fix applied to {count} token parse sites')
