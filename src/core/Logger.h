#pragma once

#include <string>
#include <string_view>

namespace alt {

enum class LogLevel { Trace, Info, Warn, Error };

// Minimal logging that mirrors the original untLogger.pas behaviour:
// append-only file under %LOCALAPPDATA%\ALTRun\altRun.log plus the debugger
// output window. Trace output is gated by Settings::trace_enabled so a normal
// user never pays for it.
class Logger {
public:
    static Logger& Instance();

    void SetEnabled(bool enabled);
    bool enabled() const;

    void Write(LogLevel level, std::string_view message);

    void Trace(std::string_view message);
    void Info(std::string_view message);
    void Warn(std::string_view message);
    void Error(std::string_view message);

private:
    Logger() = default;

    void WriteLine(std::string_view line);

    bool enabled_ = false;
    bool file_ready_ = false;
    std::wstring file_path_;
};

}  // namespace alt
