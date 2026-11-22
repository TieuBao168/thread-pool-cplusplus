#ifndef TP_CONFIG_HPP
#define TP_CONFIG_HPP

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace tp {

/// What submit()/post() do when a bounded queue is full.
enum class OverflowPolicy {
    Block,      ///< wait until a slot frees up
    Reject,     ///< submit() throws QueueFullError, post() returns false
    CallerRuns  ///< run the task immediately on the calling thread
};

/// What happens to queued tasks when the pool shuts down.
enum class ShutdownPolicy {
    Drain,   ///< run every queued task before the workers exit
    Discard  ///< drop queued tasks (their futures get broken_promise)
};

/// How worker threads are pinned to CPUs.
enum class AffinityMode {
    None,        ///< let the OS schedule freely (default)
    RoundRobin,  ///< worker i -> cpu i % hardware threads
    List         ///< worker i -> cpu_list[i % cpu_list.size()]
};

enum class LogLevel { Off, Error, Warn, Info, Debug };

/// Receives library log lines. Called from any thread; must be thread-safe.
using LogSink = std::function<void(LogLevel, const std::string&)>;

/// Thrown for unreadable files, unknown keys or invalid values.
class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * Thread pool settings.
 *
 * Every field has a usable default, so `tp::Config{}` works as-is.
 * Values can be layered, lowest to highest priority:
 *   defaults  <  config file  <  TP_* environment variables  <  code
 *
 * Config file format (INI-like, every key optional):
 *
 *     # comment            ; comment
 *     [thread_pool]        # section headers are accepted and ignored
 *     num_workers     = 4  # 0 = std::thread::hardware_concurrency()
 *     queue_capacity  = 0  # 0 = unbounded
 *     overflow_policy = block       # block | reject | caller_runs
 *     shutdown_policy = drain       # drain | discard
 *     cpu_affinity    = none        # none | round_robin | 0,2,4
 *     thread_name     = tp-worker
 *     log_level       = warn        # off | error | warn | info | debug
 *
 * Keys and enum values are case-insensitive. The legacy format
 * `NUMS_OF_WORKER 3` is still read (with a warning); `NUMS_OF_TASK_PUSHER`
 * is ignored with a warning.
 *
 * Environment overrides use the upper-cased key with a `TP_` prefix,
 * e.g. `TP_NUM_WORKERS=8`. `TP_CONFIG_FILE` names the file load() reads.
 */
struct Config {
    std::size_t num_workers = 0;
    std::size_t queue_capacity = 0;
    OverflowPolicy overflow_policy = OverflowPolicy::Block;
    ShutdownPolicy shutdown_policy = ShutdownPolicy::Drain;
    AffinityMode affinity = AffinityMode::None;
    std::vector<unsigned> cpu_list;  ///< used when affinity == List
    std::string thread_name = "tp-worker";
    LogLevel log_level = LogLevel::Warn;
    LogSink log_sink;  ///< empty = write to stderr (not settable from files)

    /// Non-fatal notes collected while parsing (deprecated keys, ...).
    std::vector<std::string> warnings;

    // Fluent setters for configuring in code.
    Config& workers(std::size_t n) { num_workers = n; return *this; }
    Config& queueCapacity(std::size_t n) { queue_capacity = n; return *this; }
    Config& onOverflow(OverflowPolicy p) { overflow_policy = p; return *this; }
    Config& onShutdown(ShutdownPolicy p) { shutdown_policy = p; return *this; }
    Config& pinRoundRobin() { affinity = AffinityMode::RoundRobin; cpu_list.clear(); return *this; }
    Config& pinTo(std::vector<unsigned> cpus) {
        affinity = cpus.empty() ? AffinityMode::None : AffinityMode::List;
        cpu_list = std::move(cpus);
        return *this;
    }
    Config& threadName(std::string name) { thread_name = std::move(name); return *this; }
    Config& logLevel(LogLevel l) { log_level = l; return *this; }
    Config& logSink(LogSink sink) { log_sink = std::move(sink); return *this; }

    /// Sets one setting from its textual form, as written in a config file.
    /// Throws ConfigError on an unknown key or invalid value.
    Config& set(const std::string& key, const std::string& value);

    /// Applies TP_* environment variables on top of the current values.
    Config& applyEnv();

    /// num_workers, or hardware_concurrency() (at least 1) when it is 0.
    std::size_t resolvedWorkers() const;

    /// Throws ConfigError if the combination of values is invalid.
    void validate() const;

    /// Parses config text. `source` is used in error messages ("file:line").
    static Config fromString(const std::string& text, const std::string& source = "<string>");

    /// Reads a config file. Throws ConfigError if it cannot be opened.
    static Config fromFile(const std::string& path);

    /**
     * The "just works" loader used by `tp::ThreadPool pool;`:
     *   1. start from defaults,
     *   2. read `path`, or $TP_CONFIG_FILE when `path` is empty
     *      (a missing file is fine only when neither was given),
     *   3. apply TP_* environment variables,
     *   4. validate().
     */
    static Config load(const std::string& path = "");
};

/// Textual names, the same spellings the config file accepts
/// (except AffinityMode::List -> "list"; files write the CPU list itself).
const char* toString(OverflowPolicy p);
const char* toString(ShutdownPolicy p);
const char* toString(AffinityMode m);
const char* toString(LogLevel l);

}  // namespace tp

#endif  // TP_CONFIG_HPP
