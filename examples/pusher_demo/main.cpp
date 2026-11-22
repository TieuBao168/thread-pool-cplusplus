// pusher_demo: the legacy demo rebuilt on the tp API.
//
//   pusher_demo [config_file] [num_pushers] [seconds]
//
// N producer threads ("task pushers") each post a random arithmetic task to
// the pool every ~200 ms until `seconds` have elapsed. The pool is then
// drained with waitIdle() and its statistics are printed.

#include "common_task.hpp"

#include <tp/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

unsigned long parseCount(const char* text, const char* what) {
    char* end = nullptr;
    unsigned long value = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value == 0) {
        throw std::invalid_argument(std::string("invalid ") + what + ": '" + text + "'");
    }
    return value;
}

std::string describeAffinity(const tp::Config& cfg) {
    std::string text = tp::toString(cfg.affinity);
    if (cfg.affinity == tp::AffinityMode::List) {
        text += " (";
        for (std::size_t i = 0; i < cfg.cpu_list.size(); ++i) {
            if (i != 0) text += ',';
            text += std::to_string(cfg.cpu_list[i]);
        }
        text += ')';
    }
    return text;
}

void printConfig(const tp::ThreadPool& pool) {
    const tp::Config& cfg = pool.config();
    std::cout << "##### Thread pool configuration #####\n"
              << "  workers         : " << pool.size() << '\n'
              << "  queue capacity  : "
              << (cfg.queue_capacity == 0 ? std::string("unbounded")
                                          : std::to_string(cfg.queue_capacity))
              << '\n'
              << "  overflow policy : " << tp::toString(cfg.overflow_policy) << '\n'
              << "  shutdown policy : " << tp::toString(cfg.shutdown_policy) << '\n'
              << "  cpu affinity    : " << describeAffinity(cfg) << '\n'
              << "  thread name     : " << cfg.thread_name << '\n'
              << "  log level       : " << tp::toString(cfg.log_level) << '\n'
              << "  config warnings : " << cfg.warnings.size() << " (logged by tp)\n"
              << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
    std::string configFile = "threadpool.ini";
    unsigned long numPushers = 1;
    unsigned long seconds = 10;

    try {
        if (argc > 1) configFile = argv[1];
        if (argc > 2) numPushers = parseCount(argv[2], "num_pushers");
        if (argc > 3) seconds = parseCount(argv[3], "seconds");
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n'
                  << "usage: " << argv[0] << " [config_file] [num_pushers] [seconds]\n";
        return 2;
    }

    std::cout << "Hardware threads: " << std::thread::hardware_concurrency() << "\n\n";

    std::unique_ptr<tp::ThreadPool> pool;
    try {
        if (fileExists(configFile)) {
            std::cout << "Loading config from '" << configFile << "'\n";
            pool = std::make_unique<tp::ThreadPool>(configFile);
        } else {
            std::cout << "Note: config file '" << configFile
                      << "' not found, using tp defaults (+ TP_* environment)\n";
            pool = std::make_unique<tp::ThreadPool>();
        }
    } catch (const tp::ConfigError& e) {
        std::cerr << "configuration error: " << e.what() << '\n';
        return 1;
    }
    printConfig(*pool);

    std::cout << "Starting " << numPushers << " task pusher(s) for " << seconds
              << " second(s)\n\n";

    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> pushed{0};
    std::atomic<std::uint64_t> refused{0};

    std::vector<std::thread> pushers;
    pushers.reserve(numPushers);
    for (unsigned long id = 0; id < numPushers; ++id) {
        pushers.emplace_back([&, id] {
            // Each producer owns its generator: no shared RNG, no data race.
            std::seed_seq seed{std::random_device{}(), static_cast<unsigned>(id)};
            std::mt19937 rng(seed);
            while (!stop.load(std::memory_order_relaxed)) {
                if (pool->post(demo::generateRandomTask(rng))) {
                    pushed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    refused.fetch_add(1, std::memory_order_relaxed);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    stop.store(true);
    for (std::thread& t : pushers) t.join();

    pool->waitIdle();

    const tp::Stats s = pool->stats();
    std::ostringstream summary;
    summary << "\n##### Summary #####\n"
            << "  tasks pushed    : " << pushed.load() << '\n'
            << "  tasks refused   : " << refused.load() << '\n'
            << "  completed       : " << s.completed << '\n'
            << "  failed          : " << s.failed << '\n'
            << "  rejected        : " << s.rejected << '\n'
            << "  pending/active  : " << s.pending << '/' << s.active << '\n';
    demo::printLine(summary.str());

    pool.reset();  // shutdown + join workers
    std::cout << "Main thread exiting" << std::endl;
    return 0;
}
