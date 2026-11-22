# tp — a configurable C++17 thread pool

`tp` is a small, dependency-free thread-pool library for C++17. It runs a fixed
set of worker threads and lets any thread hand them work, either as a typed
`std::future` or as fire-and-forget. Defaults work as-is, and every setting can
also come from a config file, environment variables or code.

## Features

- **Typed `submit()`**: returns `std::future<R>` for any callable plus its
  arguments. Exceptions thrown by the task come back out of `future::get()`.
- **Fire-and-forget `post()`**: no future to manage. Exceptions are logged and
  counted.
- **Bounded or unbounded queue** with an overflow policy: `block`, `reject` or
  `caller_runs`.
- **Shutdown policy**: `drain` runs every queued task, `discard` drops them
  (their futures get `broken_promise`).
- **Optional CPU affinity**: round-robin, or an explicit CPU list.
- **Thread naming** (`tp-worker-0`, `tp-worker-1`, ...) so workers are easy to
  find in debuggers and profilers.
- **Statistics**: pending, active, completed, failed and rejected counts.
- **Layered configuration**: built-in defaults, then an INI-style file, then
  `TP_*` environment variables, then code (fluent setters).
- **Pluggable logging**: send library messages to your own logger.
- **Cross-platform**: Linux, Windows (MSVC and MinGW) and macOS. No
  dependencies beyond the standard library and the platform thread API.

## Quick start

```cpp
#include <tp/thread_pool.hpp>
#include <iostream>

int main() {
    tp::ThreadPool pool;  // hardware_concurrency() workers, unbounded queue

    auto sum = pool.submit([](int a, int b) { return a + b; }, 20, 22);
    std::cout << sum.get() << '\n';  // 42

    pool.post([] { std::cout << "fire and forget\n"; });

    pool.waitIdle();  // wait until the queue is empty and all workers are idle
}   // the destructor shuts the pool down (drains the queue by default)
```

[`examples/basic/main.cpp`](examples/basic/main.cpp) has a longer tour.
[`examples/pusher_demo`](examples/pusher_demo) reproduces the original demo,
where producer threads push random arithmetic tasks.

## Integration

The library target is **`tp::thread_pool`** and the public header is
**`<tp/thread_pool.hpp>`**. `<tp/config.hpp>` comes with it.

### (a) CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    tp
    GIT_REPOSITORY https://github.com/TieuBao168/thread-pool-cplusplus.git
    GIT_TAG        main   # pin a release tag or commit hash for reproducible builds
)
FetchContent_MakeAvailable(tp)

target_link_libraries(my_app PRIVATE tp::thread_pool)
```

Tests, examples, benchmarks and install rules are off by default when `tp` is
not the top-level project.

### (b) add_subdirectory

```cmake
add_subdirectory(third_party/thread-pool-cplusplus)
target_link_libraries(my_app PRIVATE tp::thread_pool)
```

### (c) Install + find_package

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /opt/tp
```

```cmake
find_package(tp CONFIG REQUIRED)   # add -DCMAKE_PREFIX_PATH=/opt/tp if needed
target_link_libraries(my_app PRIVATE tp::thread_pool)
```

## Configuration

Settings are applied in layers, from lowest to highest priority:

```
built-in defaults  <  config file  <  TP_* environment variables  <  code
```

### Keys

| Key               | Values                                   | Default       | Environment variable  |
|-------------------|------------------------------------------|---------------|-----------------------|
| `num_workers`     | integer; `0` = `hardware_concurrency()`  | `0`           | `TP_NUM_WORKERS`      |
| `queue_capacity`  | integer; `0` = unbounded                 | `0`           | `TP_QUEUE_CAPACITY`   |
| `overflow_policy` | `block` \| `reject` \| `caller_runs`     | `block`       | `TP_OVERFLOW_POLICY`  |
| `shutdown_policy` | `drain` \| `discard`                     | `drain`       | `TP_SHUTDOWN_POLICY`  |
| `cpu_affinity`    | `none` \| `round_robin` \| list `0,2,4`  | `none`        | `TP_CPU_AFFINITY`     |
| `thread_name`     | base name; workers get `<name>-<index>`  | `tp-worker`   | `TP_THREAD_NAME`      |
| `log_level`       | `off` \| `error` \| `warn` \| `info` \| `debug` | `warn` | `TP_LOG_LEVEL`        |

`TP_CONFIG_FILE` names the config file to read when none is given in code.
Keys and enum values are case-insensitive. Unknown keys and invalid values
raise `tp::ConfigError`, with `file:line` in the message.

Overflow policies, which apply only when `queue_capacity > 0`:

- `block`: `submit()` and `post()` wait until a slot frees up.
- `reject`: `submit()` throws `tp::QueueFullError`, `post()` returns `false`.
- `caller_runs`: the task runs right away on the thread that submitted it.

CPU affinity is best effort. If a thread cannot be pinned, the pool logs a
warning and keeps running. macOS has no hard pinning API, so affinity is
ignored there.

### Config file

The format is INI-like. `#` and `;` start comments, section headers are
accepted and ignored, and every key is optional. A fully commented file is in
[`config/threadpool.ini.example`](config/threadpool.ini.example):

```ini
[thread_pool]
num_workers     = 4
queue_capacity  = 1024
overflow_policy = block
shutdown_policy = drain
cpu_affinity    = none
thread_name     = tp-worker
log_level       = warn
```

### How the constructors pick up configuration

```cpp
tp::ThreadPool pool;                    // Config::load(): $TP_CONFIG_FILE if set (else defaults) + TP_* env
tp::ThreadPool pool("threadpool.ini");  // Config::load("threadpool.ini") + TP_* env; ConfigError if unusable
tp::ThreadPool pool(cfg);               // exactly `cfg` (validated; environment NOT applied)
```

With the default constructor, an unset `TP_CONFIG_FILE` is fine. If you name a
file, in code or through `TP_CONFIG_FILE`, it must exist.

```bash
TP_CONFIG_FILE=/etc/myapp/threadpool.ini TP_NUM_WORKERS=8 ./my_app
```

### Configuration in code

```cpp
tp::Config cfg = tp::Config{}
                     .workers(4)
                     .queueCapacity(1000)
                     .onOverflow(tp::OverflowPolicy::Reject)
                     .onShutdown(tp::ShutdownPolicy::Discard)
                     .pinRoundRobin()          // or .pinTo({0, 2, 4})
                     .threadName("io")
                     .logLevel(tp::LogLevel::Info);
tp::ThreadPool pool(cfg);
```

To keep code as the top layer while still honouring a file and the environment,
load first and then override:

```cpp
tp::Config cfg = tp::Config::load("threadpool.ini");  // defaults < file < env
cfg.queueCapacity(256);                                // code wins
tp::ThreadPool pool(cfg);
```

Other helpers: `Config::fromFile(path)`, `Config::fromString(text)`,
`cfg.set("num_workers", "8")`, `cfg.applyEnv()`, `cfg.validate()` and
`cfg.resolvedWorkers()`. Non-fatal parse notes, such as deprecated keys, are
collected in `cfg.warnings`. `tp::toString()` turns any enum back into its
config-file spelling.

### Custom log sink

By default, library messages at or above `log_level` go to `stderr`. To route
them elsewhere, set a sink. It may be called from any thread, so it must be
thread-safe.

```cpp
auto cfg = tp::Config{}.logLevel(tp::LogLevel::Info).logSink(
    [](tp::LogLevel level, const std::string& msg) {
        my_logger.log(tp::toString(level), msg);
    });
```

## API reference

All members are thread-safe. `waitIdle()` and `shutdown()` throw
`std::logic_error` when called from one of the pool's own workers.

| Member | Description |
|--------|-------------|
| `ThreadPool()` | Uses `Config::load()`: `$TP_CONFIG_FILE` if set, else defaults, plus `TP_*` env. |
| `explicit ThreadPool(const std::string& file)` | Uses `Config::load(file)`. Throws `ConfigError` if the file is unusable. |
| `explicit ThreadPool(Config cfg)` | Uses `cfg` as given (validated; the environment is not applied). |
| `~ThreadPool()` | Calls `shutdown()` with the configured shutdown policy. |
| `submit(f, args...) -> std::future<R>` | Queues `f(args...)`. Arguments are copied or moved in; use `std::ref` to pass a reference. Exceptions from the task are rethrown by `future::get()`. Throws `QueueFullError` (with `reject`) or `PoolStoppedError`. |
| `post(f, args...) -> bool` | Queues `f(args...)` without a future. Returns `false` if the task was refused (queue full with `reject`, or pool stopped). Exceptions from the task are logged and counted in `Stats::failed`. |
| `waitIdle()` | Blocks until the queue is empty and no task is running. |
| `shutdown()` / `shutdown(ShutdownPolicy)` | Stops accepting tasks, then drains or discards the queue and joins the workers. Idempotent. |
| `isRunning() -> bool` | `false` once shutdown has started. |
| `size() -> std::size_t` | Number of worker threads. |
| `stats() -> Stats` | Snapshot of `workers`, `pending`, `active`, `completed`, `failed`, `rejected`. |
| `config() -> const Config&` | The effective configuration. |

| Exception | Thrown when |
|-----------|-------------|
| `tp::QueueFullError` | `submit()` finds a full queue under `OverflowPolicy::Reject`. |
| `tp::PoolStoppedError` | `submit()` is called after `shutdown()` has started. |
| `tp::ConfigError` | A config file cannot be opened, or a key or value is invalid. |

All three derive from `std::runtime_error`.

## Building from source

Requirements: CMake 3.16 or newer and a C++17 compiler (GCC, Clang or MSVC).

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

| CMake option | Default (top level) | Meaning |
|--------------|---------------------|---------|
| `TP_BUILD_TESTS` | `ON` | Build the unit tests (`ctest`). |
| `TP_BUILD_EXAMPLES` | `ON` | Build `tp_example_basic` and `tp_pusher_demo`. |
| `TP_BUILD_BENCHMARKS` | `ON` | Build `tp_bench_throughput`. |
| `TP_INSTALL` | `ON` | Generate install rules and the `tp` CMake package. |
| `TP_WARNINGS_AS_ERRORS` | `OFF` | Add `-Werror` or `/WX`. |
| `BUILD_SHARED_LIBS` | `OFF` | Build a shared library instead of a static one. |

When `tp` is consumed as a subproject, the first four options default to `OFF`.

### make wrapper

A thin `Makefile` wraps the CMake commands:

| Target | Action |
|--------|--------|
| `make` / `make all` | Configure and build into `build/`. |
| `make test` | Build, then run `ctest`. |
| `make run` | Run the pusher demo (`RUN_ARGS="threadpool.ini 2 5"`). |
| `make bench` | Run the throughput benchmark. |
| `make install PREFIX=/opt/tp` | Install headers, library and CMake package. |
| `make clean` | Remove the build directory. |

Variables: `BUILD_TYPE` (Release/Debug), `BUILD_DIR`, `GENERATOR`, `JOBS`,
`CMAKE_ARGS`.

### Running the programs

```bash
./build/tp_example_basic
./build/tp_pusher_demo [config_file] [num_pushers] [seconds]   # defaults: threadpool.ini 1 10
./build/tp_bench_throughput [num_tasks]                         # default 200000
```

The build copies `config/threadpool.ini.example` next to `tp_pusher_demo` as
`threadpool.ini`. If the file is missing, the demo falls back to the built-in
defaults.

### Docker

The image builds the project with warnings as errors, runs the tests, and then
runs the pusher demo:

```bash
docker build -t tp-thread-pool .
docker run --rm tp-thread-pool
```

## Project layout

```
.
├── include/tp/
│   ├── config.hpp            # tp::Config, policies, ConfigError, toString()
│   └── thread_pool.hpp       # tp::ThreadPool, Stats, QueueFullError, PoolStoppedError
├── src/
│   ├── config.cpp            # parsing, env overrides, validation
│   ├── thread_pool.cpp       # queue, workers, shutdown
│   └── platform.hpp/.cpp     # thread naming and CPU pinning per OS
├── examples/
│   ├── basic/                # API tour
│   └── pusher_demo/          # producer threads + random arithmetic tasks
├── benchmarks/
│   └── bench_throughput.cpp  # tasks/sec for several worker counts and task sizes
├── tests/                    # unit tests + an install/find_package consumer project
├── config/
│   └── threadpool.ini.example
├── cmake/
│   └── tpConfig.cmake.in     # package config for find_package(tp)
├── CMakeLists.txt
├── CMakePresets.json
├── Makefile                  # convenience wrapper around CMake
├── Dockerfile
└── componentDiagram.plantuml
```

## Migrating from the legacy API

Before this rewrite, the project was a demo with a global-style `ThreadPool`
that started its own "task pusher" threads. The old API maps to the new one as
follows:

| Legacy | New |
|--------|-----|
| `#include "thread_pool.hpp"` | `#include <tp/thread_pool.hpp>`, link `tp::thread_pool` |
| `ThreadPool pool; pool.initialize("file")` | `tp::ThreadPool pool("file");` (throws `tp::ConfigError` instead of returning `false`) |
| `pool.pushTask(static_cast<void*>(new TaskData(f, args...)))` | `pool.post(f, args...);` (no raw pointers; the pool owns the task) |
| `TaskData` / `callBackFunc` | Any callable: lambda, function, functor, or move-only type |
| No way to get a result | `auto fut = pool.submit(f, args...); fut.get();` |
| `std::this_thread::sleep_for(...)` to wait for tasks | `pool.waitIdle();` |
| `getNumWorkers()` | `pool.size()` |
| `NUMS_OF_WORKER 3` | `num_workers = 3` |
| `NUMS_OF_TASK_PUSHER 2` | Removed: producers are your application's threads (see `examples/pusher_demo`) |

Legacy config files still load. `NUMS_OF_WORKER` is read as `num_workers` with
a deprecation warning. `NUMS_OF_TASK_PUSHER` is ignored with a warning,
because the pool no longer creates producer threads. The arithmetic tasks
(`CommonTask`) now live in `examples/pusher_demo/common_task.hpp`, in
namespace `demo`.

## License

MIT. See [LICENSE](LICENSE).
