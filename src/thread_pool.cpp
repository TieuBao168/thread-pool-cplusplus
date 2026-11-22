#include "tp/thread_pool.hpp"

#include "platform.hpp"

#include <cstdio>
#include <exception>

namespace tp {

namespace {

// The pool the current thread works for, so a task cannot deadlock the pool
// by calling waitIdle()/shutdown() on it.
thread_local const ThreadPool* tlsCurrentPool = nullptr;

std::mutex& stderrMutex() {
    static std::mutex m;
    return m;
}

}  // namespace

ThreadPool::ThreadPool() : ThreadPool(Config::load()) {}

ThreadPool::ThreadPool(const std::string& configFile) : ThreadPool(Config::load(configFile)) {}

ThreadPool::ThreadPool(Config config) : config_(std::move(config)) {
    config_.validate();
    for (const auto& w : config_.warnings) log(LogLevel::Warn, w);

    const std::size_t n = config_.resolvedWorkers();
    workers_.reserve(n);
    try {
        for (std::size_t i = 0; i < n; ++i) {
            workers_.emplace_back([this, i] { workerLoop(i); });
        }
    } catch (...) {
        shutdown(ShutdownPolicy::Discard);
        throw;
    }
    log(LogLevel::Info, "started " + std::to_string(n) + " workers (queue_capacity=" +
                            std::to_string(config_.queue_capacity) + ", overflow=" +
                            toString(config_.overflow_policy) + ")");
}

ThreadPool::~ThreadPool() {
    if (onWorkerThread()) {
        // A worker cannot join itself; there is no safe way to continue.
        log(LogLevel::Error, "ThreadPool destroyed from one of its own tasks");
        std::terminate();
    }
    shutdown(config_.shutdown_policy);
}

ThreadPool::Enqueued ThreadPool::enqueue(detail::Task&& task, const Clock::time_point* deadline) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (stopping_) {
        ++rejected_;
        return Enqueued::Stopped;
    }

    const std::size_t cap = config_.queue_capacity;
    const auto hasSpace = [&] { return stopping_ || queue_.size() < cap; };

    if (cap != 0 && queue_.size() >= cap && deadline != nullptr) {
        // try*/try*For: bounded wait, independent of the overflow policy.
        // wait_until re-checks the predicate on timeout, so a notify that
        // races with the timeout still hands the slot to this producer.
        ++blockedProducers_;
        const bool gotSlot = notFull_.wait_until(lock, *deadline, hasSpace);
        --blockedProducers_;
        if (stopping_ || !gotSlot) {
            ++rejected_;
            return stopping_ ? Enqueued::Stopped : Enqueued::Rejected;
        }
    } else if (cap != 0 && queue_.size() >= cap) {
        OverflowPolicy policy = config_.overflow_policy;
        // A worker blocking on its own full queue could deadlock the pool.
        if (policy == OverflowPolicy::Block && onWorkerThread()) policy = OverflowPolicy::CallerRuns;

        switch (policy) {
            case OverflowPolicy::Reject:
                ++rejected_;
                return Enqueued::Rejected;
            case OverflowPolicy::CallerRuns:
                ++active_;  // keeps waitIdle() waiting for the inline task
                lock.unlock();
                runTask(task);
                lock.lock();
                --active_;
                if (active_ == 0 && queue_.empty()) idle_.notify_all();
                return Enqueued::RanInline;
            case OverflowPolicy::Block:
                ++blockedProducers_;
                notFull_.wait(lock, hasSpace);
                --blockedProducers_;
                if (stopping_) {
                    ++rejected_;
                    return Enqueued::Stopped;
                }
                break;
        }
    }

    queue_.push_back(std::move(task));
    const bool wake = needWakeup();
    lock.unlock();
    if (wake) notEmpty_.notify_one();
    return Enqueued::Queued;
}

void ThreadPool::workerLoop(std::size_t index) {
    tlsCurrentPool = this;

    const std::string name = config_.thread_name + "-" + std::to_string(index);
    if (!detail::setCurrentThreadName(name)) log(LogLevel::Debug, "could not name thread " + name);

    if (config_.affinity != AffinityMode::None) {
        const unsigned cpu = config_.affinity == AffinityMode::RoundRobin
                                 ? static_cast<unsigned>(index % detail::hardwareThreads())
                                 : config_.cpu_list[index % config_.cpu_list.size()];
        if (!detail::pinCurrentThread(cpu)) {
            log(LogLevel::Warn, "could not pin " + name + " to cpu " + std::to_string(cpu));
        }
    }

    std::unique_lock<std::mutex> lock(mutex_);
    for (;;) {
        if (queue_.empty()) {
            if (stopping_) break;  // Drain: leave only once the queue is empty
            // No predicate on purpose: every wakeup (real or spurious) must
            // settle pendingWakeups_; the loop re-checks the state under the lock.
            ++sleepingWorkers_;
            notEmpty_.wait(lock);
            --sleepingWorkers_;
            if (pendingWakeups_ > 0) --pendingWakeups_;
            continue;
        }

        {
            detail::Task task = std::move(queue_.front());
            queue_.pop_front();
            ++active_;
            const bool wakeProducer = blockedProducers_ > 0;
            // Fan out: if work is still queued, hand it to another sleeper.
            const bool wakeWorker = !queue_.empty() && needWakeup();
            lock.unlock();
            if (wakeProducer) notFull_.notify_one();
            if (wakeWorker) notEmpty_.notify_one();

            runTask(task);
        }  // task (and whatever it captured) is destroyed outside the lock

        lock.lock();
        --active_;
        if (active_ == 0 && queue_.empty()) idle_.notify_all();
    }
}

bool ThreadPool::needWakeup() noexcept {
    // Awake workers re-check the queue before sleeping, so only sleepers need a
    // notify, and only if the wakeups already in flight cannot cover the queue.
    // Skipping redundant notifies avoids a lock convoy with many idle workers.
    if (sleepingWorkers_ > pendingWakeups_ && pendingWakeups_ < queue_.size()) {
        ++pendingWakeups_;
        return true;
    }
    return false;
}

void ThreadPool::runTask(detail::Task& task) noexcept {
    try {
        task();
        ++completed_;
    } catch (const std::exception& e) {
        ++failed_;
        log(LogLevel::Warn, std::string("task threw: ") + e.what());
    } catch (...) {
        ++failed_;
        log(LogLevel::Warn, "task threw a non-std exception");
    }
}

void ThreadPool::waitIdle() {
    if (onWorkerThread()) throw std::logic_error("tp::ThreadPool::waitIdle() called from a worker");
    std::unique_lock<std::mutex> lock(mutex_);
    idle_.wait(lock, [this] { return queue_.empty() && active_ == 0; });
}

bool ThreadPool::waitIdleUntil(Clock::time_point deadline) {
    if (onWorkerThread()) throw std::logic_error("tp::ThreadPool::waitIdleFor() called from a worker");
    std::unique_lock<std::mutex> lock(mutex_);
    return idle_.wait_until(lock, deadline, [this] { return queue_.empty() && active_ == 0; });
}

void ThreadPool::shutdown() { shutdown(config_.shutdown_policy); }

void ThreadPool::shutdown(ShutdownPolicy policy) {
    if (onWorkerThread()) throw std::logic_error("tp::ThreadPool::shutdown() called from a worker");

    std::lock_guard<std::mutex> joinLock(joinMutex_);
    std::deque<detail::Task> dropped;
    {
        // stopping_ must change under the mutex, otherwise a worker that just
        // checked the predicate could miss the notify below and sleep forever.
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        if (policy == ShutdownPolicy::Discard) {
            dropped.swap(queue_);
            if (active_ == 0) idle_.notify_all();
        }
    }
    notEmpty_.notify_all();
    notFull_.notify_all();

    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }

    if (!dropped.empty()) {
        log(LogLevel::Info, "discarded " + std::to_string(dropped.size()) + " queued tasks");
    }
    // `dropped` dies here; submit() futures of those tasks get broken_promise.
}

bool ThreadPool::isRunning() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return !stopping_;
}

Stats ThreadPool::stats() const {
    Stats s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        s.pending = queue_.size();
        s.active = active_;
    }
    s.workers = workers_.size();
    s.completed = completed_.load();
    s.failed = failed_.load();
    s.rejected = rejected_.load();
    return s;
}

bool ThreadPool::onWorkerThread() const noexcept { return tlsCurrentPool == this; }

void ThreadPool::log(LogLevel level, const std::string& message) const {
    if (level == LogLevel::Off || config_.log_level == LogLevel::Off || level > config_.log_level) return;
    if (config_.log_sink) {
        try {
            config_.log_sink(level, message);
        } catch (...) {
            // A broken sink must not take a worker down.
        }
        return;
    }
    const std::string line = std::string("[tp] ") + toString(level) + ": " + message + "\n";
    std::lock_guard<std::mutex> lock(stderrMutex());
    std::fwrite(line.data(), 1, line.size(), stderr);
}

}  // namespace tp
