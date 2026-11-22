#include "platform.hpp"

#include <cstddef>
#include <thread>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <pthread.h>
#  if defined(__linux__)
#    include <sched.h>
#  endif
#endif

namespace tp {
namespace detail {

unsigned hardwareThreads() noexcept {
    unsigned n = std::thread::hardware_concurrency();
    return n == 0 ? 1 : n;
}

#if defined(_WIN32)

bool setCurrentThreadName(const std::string& name) noexcept {
    // SetThreadDescription exists since Windows 10 1607; resolve it at runtime
    // so the library still loads on older systems.
    using SetDescFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    if (!kernel) return false;
    auto setDesc = reinterpret_cast<SetDescFn>(
        reinterpret_cast<void*>(GetProcAddress(kernel, "SetThreadDescription")));
    if (!setDesc) return false;

    int len = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
    if (len <= 0) return false;
    std::wstring wide(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, &wide[0], len);
    return SUCCEEDED(setDesc(GetCurrentThread(), wide.c_str()));
}

bool pinCurrentThread(unsigned cpu) noexcept {
    // Only the first processor group (64 CPUs) is addressable this way.
    if (cpu >= sizeof(DWORD_PTR) * 8) return false;
    DWORD_PTR mask = static_cast<DWORD_PTR>(1) << cpu;
    return SetThreadAffinityMask(GetCurrentThread(), mask) != 0;
}

#elif defined(__linux__)

bool setCurrentThreadName(const std::string& name) noexcept {
    // The kernel limit is 16 bytes including the terminator.
    std::string shortName = name.substr(0, 15);
    return pthread_setname_np(pthread_self(), shortName.c_str()) == 0;
}

bool pinCurrentThread(unsigned cpu) noexcept {
    if (cpu >= CPU_SETSIZE) return false;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
}

#elif defined(__APPLE__)

bool setCurrentThreadName(const std::string& name) noexcept {
    return pthread_setname_np(name.substr(0, 63).c_str()) == 0;
}

bool pinCurrentThread(unsigned) noexcept {
    return false;  // macOS has no hard CPU pinning API
}

#else

bool setCurrentThreadName(const std::string&) noexcept { return false; }
bool pinCurrentThread(unsigned) noexcept { return false; }

#endif

}  // namespace detail
}  // namespace tp
