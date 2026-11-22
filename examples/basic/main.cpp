// A short tour of the tp::ThreadPool API.

#include <tp/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

std::mutex coutMutex;

void say(const std::string& line) {
    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << line << '\n';
}

int slowSquare(int x) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return x * x;
}

}  // namespace

int main() {
    // 1. Default pool: hardware_concurrency() workers, unbounded queue.
    //    Also honours $TP_CONFIG_FILE and TP_* environment variables.
    {
        tp::ThreadPool pool;
        say("[1] default pool with " + std::to_string(pool.size()) + " workers");

        // submit(): typed future for the result; extra arguments are bound.
        auto sum = pool.submit([](int a, int b) { return a + b; }, 20, 22);
        say("    submit: 20 + 22 = " + std::to_string(sum.get()));

        std::vector<std::future<int>> squares;
        for (int i = 1; i <= 8; ++i) squares.push_back(pool.submit(slowSquare, i));
        int total = 0;
        for (auto& f : squares) total += f.get();
        say("    submit x8: sum of squares 1..8 = " + std::to_string(total));

        // post(): fire-and-forget, no future. Returns false if refused.
        std::atomic<int> counter{0};
        for (int i = 0; i < 100; ++i) {
            pool.post([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
        }

        // Exceptions thrown by a submitted task are rethrown by future::get().
        auto failing = pool.submit([]() -> int { throw std::runtime_error("boom"); });
        try {
            failing.get();
        } catch (const std::exception& e) {
            say(std::string("    exception propagated through future: ") + e.what());
        }

        // waitIdle(): block until the queue is empty and nothing is running.
        pool.waitIdle();
        say("    post x100 + waitIdle: counter = " + std::to_string(counter.load()));
    }  // destructor = shutdown() with the configured policy (drain by default)

    // 2. Configuration in code with the fluent setters.
    {
        tp::Config cfg = tp::Config{}
                             .workers(2)
                             .queueCapacity(4)
                             .onOverflow(tp::OverflowPolicy::CallerRuns)
                             .onShutdown(tp::ShutdownPolicy::Drain)
                             .threadName("demo")
                             .logLevel(tp::LogLevel::Info)
                             .logSink([](tp::LogLevel level, const std::string& msg) {
                                 say(std::string("    [tp ") + tp::toString(level) + "] " + msg);
                             });
        tp::ThreadPool pool(cfg);
        say("[2] configured pool: workers=" + std::to_string(pool.size()) +
            " queue=" + std::to_string(pool.config().queue_capacity) +
            " overflow=" + tp::toString(pool.config().overflow_policy));

        // With a tiny queue and CallerRuns, overflow tasks run on this thread.
        const auto mainId = std::this_thread::get_id();
        std::atomic<int> inline_runs{0};
        for (int i = 0; i < 20; ++i) {
            pool.post([&inline_runs, mainId] {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                if (std::this_thread::get_id() == mainId) inline_runs.fetch_add(1);
            });
        }

        // A post() task that throws is logged and counted in Stats::failed.
        pool.post([] { throw std::logic_error("thrown from a posted task"); });
        pool.waitIdle();
        say("    tasks run inline by the caller (CallerRuns): " +
            std::to_string(inline_runs.load()));

        // 3. stats(): a snapshot of the pool's counters.
        const tp::Stats s = pool.stats();
        say("[3] stats: workers=" + std::to_string(s.workers) +
            " pending=" + std::to_string(s.pending) +
            " active=" + std::to_string(s.active) +
            " completed=" + std::to_string(s.completed) +
            " failed=" + std::to_string(s.failed) +
            " rejected=" + std::to_string(s.rejected));

        pool.shutdown();
        say(std::string("    after shutdown(): isRunning = ") +
            (pool.isRunning() ? "true" : "false"));
        try {
            pool.submit([] { return 0; });
        } catch (const tp::PoolStoppedError& e) {
            say(std::string("    submit after shutdown throws: ") + e.what());
        }
    }
    return 0;
}
