#include "tp/thread_pool.hpp"
#include "tp_test.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

tp::Config quiet(std::size_t workers) {
    return tp::Config{}.workers(workers).logLevel(tp::LogLevel::Off);
}

// Blocks tasks until released, so tests can fill the queue deterministically.
struct Gate {
    std::promise<void> p;
    std::shared_future<void> f = p.get_future().share();
    void open() { p.set_value(); }
    void wait() const { f.wait(); }
};

}  // namespace

TP_TEST("submit returns the task result") {
    tp::ThreadPool pool(quiet(2));
    auto f = pool.submit([](int a, int b) { return a + b; }, 2, 3);
    TP_CHECK_EQ(f.get(), 5);
}

TP_TEST("submit supports void, move-only args and std::ref") {
    tp::ThreadPool pool(quiet(2));
    auto ptr = std::make_unique<int>(41);
    auto f1 = pool.submit([](std::unique_ptr<int> p) { return *p + 1; }, std::move(ptr));
    TP_CHECK_EQ(f1.get(), 42);

    int counter = 0;
    pool.submit([](int& c) { c = 7; }, std::ref(counter)).get();
    TP_CHECK_EQ(counter, 7);
}

TP_TEST("submit propagates exceptions through the future") {
    tp::ThreadPool pool(quiet(1));
    auto f = pool.submit([]() -> int { throw std::runtime_error("boom"); });
    TP_CHECK_THROWS(f.get(), std::runtime_error);
    // The worker survives and keeps serving.
    TP_CHECK_EQ(pool.submit([] { return 1; }).get(), 1);
}

TP_TEST("post runs every task and counts failures") {
    tp::ThreadPool pool(quiet(4));
    std::atomic<int> sum{0};
    for (int i = 1; i <= 1000; ++i) TP_CHECK(pool.post([&sum, i] { sum += i; }));
    TP_CHECK(pool.post([] { throw std::runtime_error("ignored"); }));
    pool.waitIdle();
    TP_CHECK_EQ(sum.load(), 500500);
    auto s = pool.stats();
    TP_CHECK_EQ(s.completed, 1000u);
    TP_CHECK_EQ(s.failed, 1u);
    TP_CHECK_EQ(s.pending, 0u);
    TP_CHECK_EQ(s.active, 0u);
}

TP_TEST("tasks run in parallel on all workers") {
    // State shared with tasks is declared before the pool so it outlives the workers.
    std::atomic<int> running{0};
    Gate gate;
    tp::ThreadPool pool(quiet(4));
    TP_CHECK_EQ(pool.size(), 4u);
    std::vector<std::future<std::thread::id>> fs;
    for (int i = 0; i < 4; ++i) {
        fs.push_back(pool.submit([&] {
            ++running;
            gate.wait();
            return std::this_thread::get_id();
        }));
    }
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (running.load() < 4 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
    TP_CHECK_EQ(running.load(), 4);  // would be 1 with the old single-core pinning
    gate.open();
    std::set<std::thread::id> ids;
    for (auto& f : fs) ids.insert(f.get());
    TP_CHECK_EQ(ids.size(), 4u);
}

TP_TEST("destructor drains queued tasks by default") {
    std::atomic<int> done{0};
    {
        tp::ThreadPool pool(quiet(1));
        for (int i = 0; i < 100; ++i) pool.post([&] { std::this_thread::sleep_for(100us); ++done; });
    }
    TP_CHECK_EQ(done.load(), 100);
}

TP_TEST("discard shutdown drops queued tasks and breaks their futures") {
    Gate gate;
    std::promise<void> started;
    tp::ThreadPool pool(quiet(1));
    auto blocker = pool.submit([&] { started.set_value(); gate.wait(); });
    started.get_future().wait();  // the blocker is running, not queued
    std::vector<std::future<int>> queued;
    for (int i = 0; i < 10; ++i) queued.push_back(pool.submit([] { return 1; }));

    std::thread stopper([&] { pool.shutdown(tp::ShutdownPolicy::Discard); });
    std::this_thread::sleep_for(20ms);
    gate.open();
    stopper.join();

    blocker.get();  // the running task still completes
    for (auto& f : queued) TP_CHECK_THROWS(f.get(), std::future_error);
    TP_CHECK(!pool.isRunning());
}

TP_TEST("submit after shutdown throws, post returns false") {
    tp::ThreadPool pool(quiet(1));
    pool.shutdown();
    pool.shutdown();  // idempotent
    TP_CHECK_THROWS(pool.submit([] {}), tp::PoolStoppedError);
    TP_CHECK(!pool.post([] {}));
    TP_CHECK_EQ(pool.stats().rejected, 2u);
}

TP_TEST("bounded queue: Reject") {
    Gate gate;
    std::promise<void> started;
    tp::ThreadPool pool(quiet(1).queueCapacity(2).onOverflow(tp::OverflowPolicy::Reject));
    pool.post([&] { started.set_value(); gate.wait(); });
    started.get_future().wait();  // worker busy, queue empty
    TP_CHECK(pool.post([] {}));
    TP_CHECK(pool.post([] {}));
    TP_CHECK(!pool.post([] {}));
    TP_CHECK_THROWS(pool.submit([] {}), tp::QueueFullError);
    TP_CHECK_EQ(pool.stats().rejected, 2u);
    gate.open();
}

TP_TEST("bounded queue: CallerRuns executes on the caller") {
    Gate gate;
    std::promise<void> started;
    tp::ThreadPool pool(quiet(1).queueCapacity(1).onOverflow(tp::OverflowPolicy::CallerRuns));
    pool.post([&] { started.set_value(); gate.wait(); });
    started.get_future().wait();
    pool.post([] {});  // fills the queue
    auto f = pool.submit([] { return std::this_thread::get_id(); });
    TP_CHECK(f.get() == std::this_thread::get_id());
    gate.open();
}

TP_TEST("bounded queue: Block waits for space") {
    tp::ThreadPool pool(quiet(2).queueCapacity(4).onOverflow(tp::OverflowPolicy::Block));
    std::atomic<int> done{0};
    std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p) {
        producers.emplace_back([&] {
            for (int i = 0; i < 250; ++i) pool.post([&] { ++done; });
        });
    }
    for (auto& t : producers) t.join();
    pool.waitIdle();
    TP_CHECK_EQ(done.load(), 1000);
    TP_CHECK_EQ(pool.stats().rejected, 0u);
}

TP_TEST("blocked producer is released by shutdown") {
    Gate gate;
    std::promise<void> started;
    tp::ThreadPool pool(quiet(1).queueCapacity(1).onOverflow(tp::OverflowPolicy::Block));
    pool.post([&] { started.set_value(); gate.wait(); });
    started.get_future().wait();
    pool.post([] {});  // queue now full
    std::atomic<bool> result{true};
    std::thread producer([&] { result = pool.post([] {}); });
    std::this_thread::sleep_for(20ms);
    std::thread stopper([&] { pool.shutdown(tp::ShutdownPolicy::Discard); });
    producer.join();
    gate.open();
    stopper.join();
    TP_CHECK(!result.load());
}

TP_TEST("tasks may submit more tasks") {
    tp::ThreadPool pool(quiet(2));
    auto outer = pool.submit([&pool] { return pool.submit([] { return 21; }).get() * 2; });
    TP_CHECK_EQ(outer.get(), 42);
}

TP_TEST("waitIdle and shutdown from a worker throw logic_error") {
    tp::ThreadPool pool(quiet(1));
    auto f1 = pool.submit([&pool] { pool.waitIdle(); });
    TP_CHECK_THROWS(f1.get(), std::logic_error);
    auto f2 = pool.submit([&pool] { pool.shutdown(); });
    TP_CHECK_THROWS(f2.get(), std::logic_error);
}

TP_TEST("waitIdle returns immediately on an idle pool") {
    tp::ThreadPool pool(quiet(2));
    pool.waitIdle();
    TP_CHECK(pool.isRunning());
}

TP_TEST("create and destroy repeatedly without hanging") {
    // Regression test for the lost-wakeup hang in the legacy pool.
    for (int i = 0; i < 2000; ++i) {
        tp::ThreadPool pool(quiet(2));
        if (i % 2) pool.post([] {});
    }
    TP_CHECK(true);
}

TP_TEST("log sink receives task failures") {
    std::mutex m;
    std::vector<std::string> lines;
    {
        tp::ThreadPool pool(tp::Config{}.workers(1).logLevel(tp::LogLevel::Warn).logSink(
            [&](tp::LogLevel, const std::string& msg) {
                std::lock_guard<std::mutex> lock(m);
                lines.push_back(msg);
            }));
        pool.post([] { throw std::runtime_error("sink-test"); });
    }
    bool found = false;
    for (auto& l : lines) found = found || l.find("sink-test") != std::string::npos;
    TP_CHECK(found);
}

TP_TEST("cpu affinity round robin does not break execution") {
    tp::ThreadPool pool(quiet(2).pinRoundRobin());
    TP_CHECK_EQ(pool.submit([] { return 3; }).get(), 3);
}

TP_TEST("zero workers resolves to hardware concurrency") {
    tp::ThreadPool pool(quiet(0));
    TP_CHECK(pool.size() >= 1u);
    TP_CHECK_EQ(pool.size(), pool.config().resolvedWorkers());
}

TP_TEST("invalid config is rejected at construction") {
    tp::Config bad = quiet(1);
    bad.affinity = tp::AffinityMode::List;  // with an empty cpu_list
    TP_CHECK_THROWS(tp::ThreadPool{bad}, tp::ConfigError);
}

TP_TEST_MAIN
