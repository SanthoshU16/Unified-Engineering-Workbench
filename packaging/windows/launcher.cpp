#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::wstring quote_argument(const std::wstring& value) {
    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (wchar_t ch : value) {
        if (ch == L'\\') {
            ++backslashes;
        } else if (ch == L'\"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted += ch;
            backslashes = 0;
        } else {
            quoted.append(backslashes, L'\\');
            quoted += ch;
            backslashes = 0;
        }
    }
    quoted.append(backslashes * 2, L'\\');
    quoted += L'\"';
    return quoted;
}

void show_error(const wchar_t* message) {
    MessageBoxW(nullptr, message, L"Unified Engineering Workbench", MB_OK | MB_ICONERROR);
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    std::vector<wchar_t> module_path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (length == 0 || length >= module_path.size()) {
        show_error(L"Could not determine the application folder.");
        return 1;
    }

    const std::filesystem::path launcher_path(std::wstring(module_path.data(), length));
    const std::filesystem::path launcher_dir = launcher_path.parent_path();
    std::filesystem::path app_root;
    std::filesystem::path server_path;

    if (launcher_dir.filename().wstring() == L"windows-x64") {
        app_root = launcher_dir.parent_path().parent_path();
        server_path = launcher_dir / L"workbench_server.exe";
    } else {
        app_root = launcher_dir;
        server_path = app_root / L"dist" / L"windows-x64" / L"workbench_server.exe";
    }

    if (!std::filesystem::exists(server_path)) {
        show_error(L"The packaged workbench_server.exe is missing from dist\\windows-x64.");
        return 1;
    }

    std::wstring command_line = quote_argument(server_path.wstring()) +
        L" --port 8080 --web-root " + quote_argument(app_root.wstring());
    std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL started = CreateProcessW(
        server_path.c_str(),
        mutable_command.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        app_root.c_str(),
        &startup,
        &process);

    if (!started) {
        show_error(L"The workbench server could not be started. Check that the extracted folder is complete.");
        return 1;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
