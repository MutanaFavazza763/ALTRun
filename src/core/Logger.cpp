#include "core/Logger.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <mutex>

#include "core/Text.h"

namespace alt {
namespace {

std::mutex g_log_mutex;

const char* LevelTag(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Info: return "INFO ";
        case LogLevel::Warn: return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

std::wstring ExeDirectory() {
    wchar_t exe[MAX_PATH * 2] = {};
    ::GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
    std::wstring path = exe;
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.resize(slash + 1);
    }
    return path;
}

// Returns the first candidate log path that can actually be opened for append.
// Preference: %LOCALAPPDATA%\ALTRun\altRun.log, then next to the executable
// (portable, and the only writable location in sandboxed environments).
std::wstring DefaultLogPath() {
    std::wstring local_app_data;
    wchar_t buffer[MAX_PATH * 2] = {};
    const DWORD written = ::GetEnvironmentVariableW(
        L"LOCALAPPDATA", buffer, static_cast<DWORD>(std::size(buffer)));
    if (written > 0 && written < std::size(buffer)) {
        local_app_data.assign(buffer, written);
    }

    std::wstring candidates[2];
    if (!local_app_data.empty()) {
        const std::wstring dir = local_app_data + L"\\ALTRun";
        ::CreateDirectoryW(dir.c_str(), nullptr);
        candidates[0] = dir + L"\\altRun.log";
    }
    candidates[1] = ExeDirectory() + L"altRun.log";

    for (const std::wstring& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }
        HANDLE probe = ::CreateFileW(candidate.c_str(), FILE_APPEND_DATA,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                     OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (probe != INVALID_HANDLE_VALUE) {
            ::CloseHandle(probe);
            return candidate;
        }
    }
    return std::wstring();
}

std::string Timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch())
                        .count() %
                    1000;
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    ::localtime_s(&local, &t);
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                  local.tm_hour, local.tm_min, local.tm_sec,
                  static_cast<int>(ms));
    return buffer;
}

}  // namespace

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

void Logger::SetEnabled(bool enabled) {
    std::lock_guard lock(g_log_mutex);
    enabled_ = enabled;
}

bool Logger::enabled() const {
    return enabled_;
}

void Logger::Write(LogLevel level, std::string_view message) {
    if (level == LogLevel::Trace && !enabled_) {
        return;
    }
    WriteLine(std::string("[" ) + Timestamp() + "] [" + LevelTag(level) + "] " +
              std::string(message));
}

void Logger::Trace(std::string_view message) { Write(LogLevel::Trace, message); }
void Logger::Info(std::string_view message) { Write(LogLevel::Info, message); }
void Logger::Warn(std::string_view message) { Write(LogLevel::Warn, message); }
void Logger::Error(std::string_view message) { Write(LogLevel::Error, message); }

void Logger::WriteLine(std::string_view line) {
    std::lock_guard lock(g_log_mutex);

    std::string with_newline(line);
    with_newline.push_back('\n');

    const std::wstring wide = ToWide(with_newline);
    ::OutputDebugStringW(wide.c_str());

    if (!file_ready_) {
        file_path_ = DefaultLogPath();
        file_ready_ = !file_path_.empty();
    }
    if (!file_ready_) {
        return;
    }

    HANDLE file = ::CreateFileW(file_path_.c_str(), FILE_APPEND_DATA,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        file_ready_ = false;
        return;
    }
    DWORD written = 0;
    ::WriteFile(file, with_newline.data(),
                static_cast<DWORD>(with_newline.size()), &written, nullptr);
    ::CloseHandle(file);
}

}  // namespace alt
