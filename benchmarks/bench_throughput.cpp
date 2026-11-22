// Throughput benchmark: tasks/second for several worker counts and task sizes.
//
//   bench_throughput [num_tasks]     (default 200000 empty tasks)
//
// Heavier task kinds use proportionally fewer tasks so each run stays short.
// Every run posts all tasks with post() and then waits with waitIdle().

#include <tp/thread_pool.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// Results of the busy work go here so the optimizer cannot remove it.
std::atomic<std::uint64_t> g_sink{0};

// Read through a volatile so the iteration count is not a compile-time constant.
volatile std::uint32_t g_itersPerMicro = 1;

std::uint64_t spin(std::uint64_t iterations) {
    std::uint64_t x = 0x9E3779B97F4A7C15ull;
    for (std::uint64_t i = 0; i < iterations; ++i) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
    }
    return x;
}

// Measures how many spin() iterations take one microsecond on this machine.
std::uint32_t calibrate() {
    std::uint64_t iterations = 1 << 16;
    for (;;) {
        const auto t0 = Clock::now();
        g_sink.fetch_add(spin(iterations), std::memory_order_relaxed);
        const auto us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
        if (us >= 20000) {  // at least 20 ms for a stable estimate
            return static_cast<std::uint32_t>(std::max<std::uint64_t>(1, iterations / us));
        }
        iterations *= 2;
    }
}

struct Kind {
    const char* name;
    unsigned micros;  // 0 = empty task
    std::size_t divisor;  // num_tasks / divisor tasks are run
};

double runOnce(tp::ThreadPool& pool, const Kind& kind, std::size_t tasks) {
    const std::uint64_t iterations =
        static_cast<std::uint64_t>(kind.micros) * g_itersPerMicro;
    const auto t0 = Clock::now();
    if (kind.micros == 0) {
        for (std::size_t i = 0; i < tasks; ++i) pool.post([] {});
    } else {
        for (std::size_t i = 0; i < tasks; ++i) {
            pool.post([iterations] {
                g_sink.fetch_add(spin(iterations), std::memory_order_relaxed);
            });
        }
    }
    pool.waitIdle();
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

}  // namespace

int main(int argc, char** argv) {
    std::size_t numTasks = 200000;
    if (argc > 1) {
        char* end = nullptr;
        const unsigned long long n = std::strtoull(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || n == 0) {
            std::fprintf(stderr, "usage: %s [num_tasks]\n", argv[0]);
            return 2;
        }
        numTasks = static_cast<std::size_t>(n);
    }

    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::size_t> workerCounts{1, 2, 4, hw};
    std::sort(workerCounts.begin(), workerCounts.end());
    workerCounts.erase(std::unique(workerCounts.begin(), workerCounts.end()),
                       workerCounts.end());

    g_itersPerMicro = calibrate();

    const Kind kinds[] = {
        {"empty", 0, 1},
        {"~1us", 1, 4},
        {"~50us", 50, 100},
    };

    std::printf("tp::ThreadPool throughput  (hardware threads: %u, %u spin iters/us)\n\n",
                hw, static_cast<unsigned>(g_itersPerMicro));
    std::printf("%8s  %-6s  %10s  %10s  %14s\n", "workers", "task", "tasks", "time(ms)",
                "tasks/sec");
    std::printf("%8s  %-6s  %10s  %10s  %14s\n", "-------", "------", "----------",
                "----------", "--------------");

    for (std::size_t workers : workerCounts) {
        tp::ThreadPool pool(tp::Config{}.workers(workers).logLevel(tp::LogLevel::Error));
        for (const Kind& kind : kinds) {
            const std::size_t tasks = std::max<std::size_t>(1, numTasks / kind.divisor);
            runOnce(pool, kind, std::min<std::size_t>(tasks, 1000));  // warm-up
            const double secs = runOnce(pool, kind, tasks);
            std::printf("%8zu  %-6s  %10zu  %10.1f  %14.0f\n", workers, kind.name, tasks,
                        secs * 1000.0, static_cast<double>(tasks) / secs);
        }
    }

    std::printf("\n(sink: %llu)\n", static_cast<unsigned long long>(g_sink.load()));
    return 0;
}
