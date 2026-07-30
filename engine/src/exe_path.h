#pragma once
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

inline std::filesystem::path get_exe_dir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH)
        return std::filesystem::current_path();
    return std::filesystem::path(buf).parent_path();
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len == -1)
        return std::filesystem::current_path();
    buf[len] = '\0';
    return std::filesystem::path(buf).parent_path();
#endif
}