// Line-buffered log file next to the mod DLL.
#pragma once
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <string>

namespace crab {

// Directory of the DLL containing this code, with trailing backslash.
inline std::wstring ModuleDir() {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ModuleDir), &self);
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring p(path, n);
    return p.substr(0, p.find_last_of(L"\\/") + 1);
}

class Log {
public:
    static Log& Get() {
        static Log log;
        return log;
    }
    void Open(const std::wstring& path) {
        if (f_) fclose(f_);
        f_ = _wfopen(path.c_str(), L"w");
    }
    void Write(const char* fmt, ...) {
        if (!f_) return;
        SYSTEMTIME t;
        GetLocalTime(&t);
        fprintf(f_, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
        va_list ap;
        va_start(ap, fmt);
        vfprintf(f_, fmt, ap);
        va_end(ap);
        fputc('\n', f_);
        fflush(f_);
    }

private:
    FILE* f_ = nullptr;
};

}  // namespace crab

#define CRAB_LOG(...) ::crab::Log::Get().Write(__VA_ARGS__)
