#include "app/ShellIntegration.h"

#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <shlobj.h>

#include <cstddef>

#include "core/Logger.h"
#include "core/Text.h"

namespace alt {
namespace {

constexpr wchar_t kSendToShortcutName[] = L"ALTRun.lnk";
constexpr wchar_t kSendToDescription[] = L"ALTRun";

std::wstring DirectoryOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return {};
    }
    return path.substr(0, slash);
}

// Creates/overwrites link_path with a shell link pointing at exe_path.
bool CreateShortcut(const std::wstring& link_path, const std::wstring& exe_path) {
    IShellLinkW* shell_link = nullptr;
    HRESULT result = ::CoCreateInstance(CLSID_ShellLink, nullptr,
                                       CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shell_link));
    if (FAILED(result) || shell_link == nullptr) {
        Logger::Instance().Warn(std::string("sendto: CoCreateInstance(ShellLink) failed, hr=0x") +
                                std::to_string(static_cast<unsigned long>(result)));
        return false;
    }

    shell_link->SetPath(exe_path.c_str());
    const std::wstring working_directory = DirectoryOf(exe_path);
    if (!working_directory.empty()) {
        shell_link->SetWorkingDirectory(working_directory.c_str());
    }
    shell_link->SetDescription(kSendToDescription);
    // Reuse the executable's own icon (the original did the same).
    shell_link->SetIconLocation(exe_path.c_str(), 0);

    IPersistFile* persist = nullptr;
    result = shell_link->QueryInterface(IID_PPV_ARGS(&persist));
    if (SUCCEEDED(result) && persist != nullptr) {
        result = persist->Save(link_path.c_str(), TRUE);
        persist->Release();
    }
    shell_link->Release();

    if (FAILED(result)) {
        Logger::Instance().Warn("sendto: cannot save " + ToUtf8(link_path) +
                                ", hr=" + std::to_string(static_cast<unsigned long>(result)));
        return false;
    }
    return true;
}

}  // namespace

std::wstring ShellIntegration::ExecutablePath() {
    wchar_t buffer[MAX_PATH * 2] = {};
    ::GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    return buffer;
}

std::wstring ShellIntegration::ExecutableDirectory() {
    std::wstring path = ExecutablePath();
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.resize(slash + 1);
    }
    return path;
}

std::wstring ShellIntegration::SendToShortcutPath() {
    wchar_t app_data[MAX_PATH * 2] = {};
    if (FAILED(::SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr,
                                  SHGFP_TYPE_CURRENT, app_data))) {
        Logger::Instance().Warn("sendto: cannot resolve %APPDATA%");
        return {};
    }
    return std::wstring(app_data) + L"\\Microsoft\\Windows\\SendTo\\" +
           kSendToShortcutName;
}

bool ShellIntegration::ApplyAddToSendTo(bool enabled, const std::wstring& exe_path) {
    const std::wstring link_path = SendToShortcutPath();
    if (link_path.empty()) {
        return false;
    }

    if (!enabled) {
        if (::DeleteFileW(link_path.c_str()) != FALSE) {
            Logger::Instance().Info("sendto: removed " + ToUtf8(link_path));
            return true;
        }
        const DWORD error = ::GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return true;  // already absent
        }
        Logger::Instance().Warn("sendto: cannot delete " + ToUtf8(link_path) +
                                ", error=" + std::to_string(error));
        return false;
    }

    const std::wstring target = exe_path.empty() ? ExecutablePath() : exe_path;
    if (target.empty()) {
        return false;
    }
    // Rebuild instead of trusting the existing shortcut so a moved executable
    // is always reflected (the entry is idempotent).
    if (::DeleteFileW(link_path.c_str()) == FALSE) {
        const DWORD error = ::GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            Logger::Instance().Warn("sendto: cannot replace " + ToUtf8(link_path) +
                                    ", error=" + std::to_string(error));
            // Keep going: Save() may still overwrite it.
        }
    }
    const std::wstring directory = DirectoryOf(link_path);
    if (!directory.empty()) {
        ::CreateDirectoryW(directory.c_str(), nullptr);  // best effort
    }
    if (!CreateShortcut(link_path, target)) {
        return false;
    }
    Logger::Instance().Info("sendto: created " + ToUtf8(link_path) + " -> " +
                            ToUtf8(target));
    return true;
}

void ShellIntegration::PlayPopupSound(bool enabled) {
    if (!enabled) {
        return;
    }
    const std::wstring path = ExecutableDirectory() + L"Popup.wav";
    if (::GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        Logger::Instance().Trace("sound: Popup.wav not found, skip");
        return;
    }
    ::PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    Logger::Instance().Trace("sound: playing Popup.wav");
}

}  // namespace alt
