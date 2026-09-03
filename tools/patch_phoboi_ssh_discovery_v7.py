from pathlib import Path
import re

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

marker = "PHOBOI_SSH_DISCOVERY_V7"
if marker in text:
    print("PhoBoi SSH discovery V7 already applied")
    raise SystemExit(0)

for required in (
    "PHOBOI_PUBLIC_FAILOVER_V6",
    "PHOBOI_LOCALHOST_RUN_FALLBACK_V6",
):
    if required not in text:
        raise SystemExit(f"PhoBoi SSH discovery V7 requires {required}")

pattern = re.compile(
    r'std::string mobileWindowsSshPath\(\)\n\{.*?^\}\n\n(?=bool mobileExtractLocalhostRunUrl)',
    re.S | re.M,
)
match = pattern.search(text)
if match is None:
    raise SystemExit("V6 mobileWindowsSshPath function not found")

replacement = r'''// PHOBOI_SSH_DISCOVERY_V7
std::string mobileWindowsSshPath()
{
    auto usableFile = [](const std::string& candidate) -> bool {
        if (candidate.empty()) {
            return false;
        }
        DWORD attrs = GetFileAttributesA(candidate.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    };

    auto envPath = [&](const char* variable, const char* suffix) -> std::string {
        char value[32768] {};
        DWORD length = GetEnvironmentVariableA(variable, value, static_cast<DWORD>(sizeof(value)));
        if (length == 0 || length >= sizeof(value)) {
            return "";
        }
        std::string candidate(value, length);
        if (!candidate.empty() && candidate.back() != '\\' && candidate.back() != '/') {
            candidate.push_back('\\');
        }
        candidate += suffix;
        return usableFile(candidate) ? candidate : "";
    };

    // First accept a future portable/bundled OpenSSH next to the game. This
    // keeps the runtime compatible with an installer-bundled client without
    // changing the tunnel launcher again.
    char modulePath[MAX_PATH] {};
    DWORD moduleLength = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (moduleLength > 0 && moduleLength < MAX_PATH) {
        std::string base(modulePath, moduleLength);
        size_t separator = base.find_last_of("\\/");
        if (separator != std::string::npos) {
            base.resize(separator + 1);
            std::string bundled = base + "OpenSSH\\ssh.exe";
            if (usableFile(bundled)) {
                debugPrint("[PHOBOI FALLBACK] SSH source=bundled path=%s\n", bundled.c_str());
                return bundled;
            }
            bundled = base + "ssh.exe";
            if (usableFile(bundled)) {
                debugPrint("[PHOBOI FALLBACK] SSH source=app-dir path=%s\n", bundled.c_str());
                return bundled;
            }
        }
    }

    // Windows 10/11 optional OpenSSH Client.
    char systemDirectory[MAX_PATH] {};
    UINT systemLength = GetSystemDirectoryA(systemDirectory, MAX_PATH);
    if (systemLength > 0 && systemLength < MAX_PATH) {
        std::string candidate(systemDirectory, systemLength);
        if (!candidate.empty() && candidate.back() != '\\') {
            candidate.push_back('\\');
        }
        candidate += "OpenSSH\\ssh.exe";
        if (usableFile(candidate)) {
            debugPrint("[PHOBOI FALLBACK] SSH source=windows path=%s\n", candidate.c_str());
            return candidate;
        }
    }

    // PATH may contain Windows OpenSSH, Git for Windows, Scoop, Chocolatey, or
    // another user-managed OpenSSH client.
    char resolved[32768] {};
    DWORD found = SearchPathA(nullptr, "ssh.exe", nullptr, static_cast<DWORD>(sizeof(resolved)), resolved, nullptr);
    if (found > 0 && found < sizeof(resolved) && usableFile(resolved)) {
        debugPrint("[PHOBOI FALLBACK] SSH source=PATH path=%s\n", resolved);
        return std::string(resolved, found);
    }

    // Git for Windows includes a working OpenSSH client even when the optional
    // Windows OpenSSH feature is disabled. Check its normal machine and per-user
    // install locations explicitly because Git's usr\\bin is not always on PATH.
    for (const char* variable : { "ProgramFiles", "ProgramW6432", "ProgramFiles(x86)" }) {
        std::string candidate = envPath(variable, "Git\\usr\\bin\\ssh.exe");
        if (!candidate.empty()) {
            debugPrint("[PHOBOI FALLBACK] SSH source=git path=%s\n", candidate.c_str());
            return candidate;
        }
    }

    std::string candidate = envPath("LOCALAPPDATA", "Programs\\Git\\usr\\bin\\ssh.exe");
    if (!candidate.empty()) {
        debugPrint("[PHOBOI FALLBACK] SSH source=git-user path=%s\n", candidate.c_str());
        return candidate;
    }

    candidate = envPath("USERPROFILE", "scoop\\apps\\git\\current\\usr\\bin\\ssh.exe");
    if (!candidate.empty()) {
        debugPrint("[PHOBOI FALLBACK] SSH source=scoop-git path=%s\n", candidate.c_str());
        return candidate;
    }

    debugPrint("[PHOBOI FALLBACK] SSH discovery exhausted: Windows OpenSSH/Git/portable client not found\n");
    return "";
}

'''
text = text[:match.start()] + replacement + text[match.end():]

if marker not in text:
    raise SystemExit("missing V7 SSH discovery marker")
if 'Git\\\\usr\\\\bin\\\\ssh.exe' not in text:
    raise SystemExit("V7 Git for Windows SSH path missing")
if 'OpenSSH\\\\ssh.exe' not in text:
    raise SystemExit("V7 bundled/Windows OpenSSH path missing")

path.write_text(text, encoding="utf-8")
print("Applied PhoBoi V7 SSH discovery: bundled, Windows OpenSSH, PATH, Git for Windows, and Scoop")
