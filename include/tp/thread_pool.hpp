#ifndef TP_THREAD_POOL_HPP
#define TP_THREAD_POOL_HPP

#include "tp/config.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace tp {

/// submit() on a full queue with OverflowPolicy::Reject.
class QueueFullError : public std::runtime_error {
public:
    QueueFullError() : std::runtime_error("tp::ThreadPool: task queue is full") {}
};

/// submit() after shutdown() has started.
class PoolStoppedError : public std::runtime_error {
public:
    PoolStoppedError() : std::runtime_error("tp::ThreadPool: pool is stopped") {}
};

/// A point-in-time snapshot of pool counters.
struct Stats {
    std::size_t workers = 0;     ///< worker threads started (unchanged by shutdown)
    std::size_t pending = 0;     ///< tasks waiting in the queue
    std::size_t active = 0;      ///< tasks running right now (workers + caller_runs)
    std::uint64_t completed = 0; ///< tasks that ran; a submit() task that threw counts
                                 ///< here, its exception goes to the future
    std::uint64_t failed = 0;    ///< post() tasks that threw
    std::uint64_t rejected = 0;  ///< tasks refused (queue full or stopped)
};

namespace detail {

/// Move-only type-erased `void()` callable (std::function needs copyable).
class Task {
public:
    Task() = default;

    template <class F, class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Task>>>
    Task(F&& f) : impl_(std::make_unique<Impl<std::decay_t<F>>>(std::forward<F>(f))) {}

    Task(Task&&) noexcept = default;
    Task& operator=(Task&&) noexcept = default;
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    void operator()() { impl_->call(); }
    explicit operator bool() const noexcept { return impl_ != nullptr; }

private:
    struct Base {
        virtual ~Base() = default;
        virtual void call() = 0;
    };
    template <class F>
    struct Impl final : Base {
        template <class G>
        explicit Impl(G&& g) : fn(std::forward<G>(g)) {}
        void call() override { fn(); }
        F fn;
    };
    std::unique_ptr<Base> impl_;
};

}  // namespace detail

/**
 * Fixed-size thread pool.
 *
 *     tp::ThreadPool pool;                                  // defaults + TP_* env
 *     tp::ThreadPool pool("threadpool.ini");                // from a file
 *     tp::ThreadPool pool(tp::Config{}.workers(4));         // in code
 *
 *     auto f = pool.submit([](int a, int b) { return a + b; }, 1, 2);
 *     int sum = f.get();                                    // 3
 *     pool.post([] { doWork(); });                          // fire-and-forget
 *     pool.waitIdle();
 *
 * Thread-safe: submit/post/waitIdle/stats may be called from any thread,
 * including from inside a task (except waitIdle and shutdown, which throw
 * std::logic_error when called from one of this pool's workers).
 * The destructor calls shutdown() with the configured policy.
 */
class ThreadPool {
public:
    /// Config::load(): $TP_CONFIG_FILE if set, else defaults; plus TP_* env.
    ThreadPool();
    /// Config::load(configFile). Throws ConfigError if the file is unusable.
    explicit ThreadPool(const std::string& configFile);
    /// A null pointer behaves like the default constructor.
    explicit ThreadPool(const char* configFile)
        : ThreadPool(Config::load(configFile ? configFile : "")) {}
    /// Uses `config` as given (validated, env is NOT applied).
    explicit ThreadPool(Config config);

    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * Queues `f(args...)` and returns a future for its result.
     * Arguments are copied/moved into the task (use std::ref to pass a reference).
     * An exception thrown by the task is rethrown by future::get().
     * Throws QueueFullError (Reject policy) or PoolStoppedError.
     */
    template <class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

    /**
     * Queues `f(args...)` without a future. Exceptions it throws are logged
     * and counted in Stats::failed. Returns false if the task was refused.
     */
    template <class F, class... Args>
    bool post(F&& f, Args&&... args);

    /// Blocks until the queue is empty and no task is running, including
    /// tasks run inline by caller_runs. Producers still blocked on a full
    /// queue have not queued their task yet and are not waited for.
    void waitIdle();

    /// Stops accepting tasks and joins the workers. Idempotent.
    void shutdown();
    void shutdown(ShutdownPolicy policy);

    bool isRunning() const noexcept;
    /// Number of worker threads started; unchanged by shutdown() (see isRunning()).
    std::size_t size() const noexcept { return workers_.size(); }
    Stats stats() const;
    const Config& config() const noexcept { return config_; }

private:
    enum class Enqueued { Queued, RanInline, Rejected, Stopped };

    template <class F, class... Args>
    static auto bind(F&& f, Args&&... args);

    Enqueued enqueue(detail::Task&& task);
    void workerLoop(std::size_t index);
    bool needWakeup() noexcept;  // call with mutex_ held
    void runTask(detail::Task& task) noexcept;
    bool onWorkerThread() const noexcept;
    void log(LogLevel level, const std::string& message) const;

    Config config_;

    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;  // workers wait for tasks / stop
    std::condition_variable notFull_;   // producers wait (Block policy)
    std::condition_variable idle_;      // waitIdle()
    std::deque<detail::Task> queue_;
    std::size_t active_ = 0;
    std::size_t sleepingWorkers_ = 0;
    std::size_t pendingWakeups_ = 0;  // notifies sent but not yet consumed
    std::size_t blockedProducers_ = 0;
    bool stopping_ = false;

    std::mutex joinMutex_;  // serializes shutdown()
    std::vector<std::thread> workers_;

    std::atomic<std::uint64_t> completed_{0};
    std::atomic<std::uint64_t> failed_{0};
    std::atomic<std::uint64_t> rejected_{0};
};

// ---- template implementation -------------------------------------------

template <class F, class... Args>
auto ThreadPool::bind(F&& f, Args&&... args) {
    // Same semantics as std::async: decay-copy everything and invoke once with
    // rvalues, so the call matches invoke_result_t<decay_t<F>, decay_t<Args>...>
    // (std::reference_wrapper arguments are passed through, not unwrapped).
    return [fn = std::forward<F>(f),
            tup = std::tuple<std::decay_t<Args>...>(std::forward<Args>(args)...)]() mutable
           -> decltype(auto) { return std::apply(std::move(fn), std::move(tup)); };
}

template <class F, class... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
    using R = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
    std::packaged_task<R()> task(bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<R> future = task.get_future();
    switch (enqueue(detail::Task(std::move(task)))) {
        case Enqueued::Rejected: throw QueueFullError();
        case Enqueued::Stopped: throw PoolStoppedError();
        default: break;
    }
    return future;
}

template <class F, class... Args>
bool ThreadPool::post(F&& f, Args&&... args) {
    Enqueued r = enqueue(detail::Task(bind(std::forward<F>(f), std::forward<Args>(args)...)));
    return r == Enqueued::Queued || r == Enqueued::RanInline;
}

}  // namespace tp

#endif  // TP_THREAD_POOL_HPP
