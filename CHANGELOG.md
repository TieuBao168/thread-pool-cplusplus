# Changelog

All notable changes to this project. Versions follow [Semantic Versioning](https://semver.org/).

## [1.1.0] - 2026-11-22

### Added
- `tryPost()` / `trySubmit()`: queue a task only if a slot is free, never block or throw.
- `tryPostFor(timeout, ...)` / `trySubmitFor(timeout, ...)`: wait at most `timeout` for a slot.
- `waitIdleFor(timeout)`: bounded `waitIdle()`, returns whether the pool became idle.

## [1.0.0] - 2026-11-22

First release as a reusable library (`tp::thread_pool`).

### Added
- `submit()` returning `std::future`, `post()`, bounded queue with `block` / `reject` /
  `caller_runs`, `drain` / `discard` shutdown, `stats()`, log sink.
- `tp::Config`: defaults < INI file < `TP_*` environment < code, with `file:line` errors.
- CMake package (`FetchContent`, `add_subdirectory`, `find_package(tp)`), CI on
  Linux/Windows/macOS with TSan and ASan+UBSan, throughput benchmark.

### Fixed
- Shutdown hang (lost wakeup), all workers pinned to one core, RNG data race,
  task exceptions terminating the process.

### Changed
- **Breaking:** `pushTask(void*)`, `TaskData`, `initialize()` and `ConfigLoader` were
  replaced; see "Migrating from the old API" in the README.

[1.1.0]: https://github.com/TieuBao168/thread-pool-cplusplus/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/TieuBao168/thread-pool-cplusplus/releases/tag/v1.0.0
