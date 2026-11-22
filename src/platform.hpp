#ifndef TP_PLATFORM_HPP
#define TP_PLATFORM_HPP

// Private OS helpers for the calling thread. Not installed.

#include <string>

namespace tp {
namespace detail {

/// Logical CPUs available, at least 1.
unsigned hardwareThreads() noexcept;

/// Best-effort thread name (visible in debuggers / top -H). Linux truncates to 15 chars.
bool setCurrentThreadName(const std::string& name) noexcept;

/// Pins the calling thread to one logical CPU. Returns false if unsupported or it failed.
bool pinCurrentThread(unsigned cpu) noexcept;

}  // namespace detail
}  // namespace tp

#endif  // TP_PLATFORM_HPP
