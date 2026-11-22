#include "tp/config.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "tp_test.hpp"

namespace {

void setEnv(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void unsetEnv(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");  // an empty value removes the variable on Windows
#else
    unsetenv(name);
#endif
}

// Sets an environment variable for the lifetime of the guard.
struct EnvGuard {
    const char* name;
    EnvGuard(const char* n, const char* v) : name(n) { setEnv(n, v); }
    ~EnvGuard() { unsetEnv(name); }
    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;
};

// Writes `text` to a unique file in the system temp dir; removed on destruction.
struct TempFile {
    std::filesystem::path path;
    explicit TempFile(const std::string& text, const char* name = "tp_test_config.ini") {
        path = std::filesystem::temp_directory_path() / name;
        std::ofstream(path, std::ios::binary) << text;
    }
    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    std::string str() const { return path.string(); }
};

// Returns the ConfigError message thrown by `fn`, or "" if nothing was thrown.
template <class F>
std::string errorOf(F fn) {
    try {
        fn();
    } catch (const tp::ConfigError& e) {
        return e.what();
    }
    return "";
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

const char* const kAllEnv[] = {"TP_NUM_WORKERS",  "TP_QUEUE_CAPACITY", "TP_OVERFLOW_POLICY",
                               "TP_SHUTDOWN_POLICY", "TP_CPU_AFFINITY", "TP_THREAD_NAME",
                               "TP_LOG_LEVEL",    "TP_CONFIG_FILE"};

void clearEnv() {
    for (const char* v : kAllEnv) unsetEnv(v);
}

}  // namespace

using tp::AffinityMode;
using tp::Config;
using tp::ConfigError;
using tp::LogLevel;
using tp::OverflowPolicy;
using tp::ShutdownPolicy;

TP_TEST("defaults") {
    Config c;
    TP_CHECK_EQ(c.num_workers, 0u);
    TP_CHECK_EQ(c.queue_capacity, 0u);
    TP_CHECK(c.overflow_policy == OverflowPolicy::Block);
    TP_CHECK(c.shutdown_policy == ShutdownPolicy::Drain);
    TP_CHECK(c.affinity == AffinityMode::None);
    TP_CHECK(c.cpu_list.empty());
    TP_CHECK_EQ(c.thread_name, std::string("tp-worker"));
    TP_CHECK(c.log_level == LogLevel::Warn);
    TP_CHECK(!c.log_sink);
    TP_CHECK(c.warnings.empty());
    c.validate();  // must not throw
}

TP_TEST("resolvedWorkers") {
    Config c;
    TP_CHECK(c.resolvedWorkers() >= 1);
    c.workers(7);
    TP_CHECK_EQ(c.resolvedWorkers(), 7u);
}

TP_TEST("integer keys") {
    Config c;
    c.set("num_workers", "8").set("queue_capacity", "1024");
    TP_CHECK_EQ(c.num_workers, 8u);
    TP_CHECK_EQ(c.queue_capacity, 1024u);
    c.set("num_workers", "  0 ");
    TP_CHECK_EQ(c.num_workers, 0u);

    for (const char* bad : {"-1", "abc", "4abc", "", "1.5", "+3", "4 4",
                            "99999999999999999999999999999"}) {
        TP_CHECK_THROWS(c.set("num_workers", bad), ConfigError);
        TP_CHECK_THROWS(c.set("queue_capacity", bad), ConfigError);
    }
    TP_CHECK_EQ(c.num_workers, 0u);  // unchanged by failed sets
}

TP_TEST("set error message has no file:line") {
    const std::string msg = errorOf([] { Config().set("num_workers", "abc"); });
    TP_CHECK_EQ(msg, std::string("invalid value 'abc' for num_workers (expected a non-negative integer)"));
    TP_CHECK_EQ(errorOf([] { Config().set("num_wrkers", "1"); }), std::string("unknown key 'num_wrkers'"));
}

TP_TEST("overflow_policy") {
    Config c;
    c.set("overflow_policy", "reject");
    TP_CHECK(c.overflow_policy == OverflowPolicy::Reject);
    c.set("overflow_policy", "CALLER_RUNS");
    TP_CHECK(c.overflow_policy == OverflowPolicy::CallerRuns);
    c.set("overflow_policy", "Block");
    TP_CHECK(c.overflow_policy == OverflowPolicy::Block);
    TP_CHECK_THROWS(c.set("overflow_policy", "drop"), ConfigError);
    TP_CHECK_THROWS(c.set("overflow_policy", ""), ConfigError);
}

TP_TEST("shutdown_policy") {
    Config c;
    c.set("shutdown_policy", "discard");
    TP_CHECK(c.shutdown_policy == ShutdownPolicy::Discard);
    c.set("shutdown_policy", "DRAIN");
    TP_CHECK(c.shutdown_policy == ShutdownPolicy::Drain);
    TP_CHECK_THROWS(c.set("shutdown_policy", "now"), ConfigError);
}

TP_TEST("log_level") {
    Config c;
    const std::pair<const char*, LogLevel> cases[] = {
        {"off", LogLevel::Off},   {"error", LogLevel::Error}, {"WARN", LogLevel::Warn},
        {"Info", LogLevel::Info}, {"debug", LogLevel::Debug},
    };
    for (const auto& kv : cases) {
        c.set("log_level", kv.first);
        TP_CHECK(c.log_level == kv.second);
    }
    TP_CHECK_THROWS(c.set("log_level", "verbose"), ConfigError);
}

TP_TEST("thread_name") {
    Config c;
    c.set("thread_name", "  my pool  ");
    TP_CHECK_EQ(c.thread_name, std::string("my pool"));
    TP_CHECK_THROWS(c.set("thread_name", "   "), ConfigError);
    TP_CHECK_EQ(c.thread_name, std::string("my pool"));
}

TP_TEST("cpu_affinity") {
    Config c;
    c.set("cpu_affinity", "round_robin");
    TP_CHECK(c.affinity == AffinityMode::RoundRobin);
    c.set("cpu_affinity", "0,2, 4");
    TP_CHECK(c.affinity == AffinityMode::List);
    TP_CHECK(c.cpu_list == (std::vector<unsigned>{0, 2, 4}));
    c.set("cpu_affinity", "7");
    TP_CHECK(c.cpu_list == (std::vector<unsigned>{7}));
    c.set("cpu_affinity", "NONE");
    TP_CHECK(c.affinity == AffinityMode::None);
    TP_CHECK(c.cpu_list.empty());

    for (const char* bad : {"", "0,,2", "0,", ",1", "-1", "a,b", "1;2", "99999999999", "all"})
        TP_CHECK_THROWS(c.set("cpu_affinity", bad), ConfigError);
}

TP_TEST("keys are case-insensitive") {
    Config c;
    c.set("NUM_WORKERS", "3").set("Queue_Capacity", "5");
    TP_CHECK_EQ(c.num_workers, 3u);
    TP_CHECK_EQ(c.queue_capacity, 5u);
}

TP_TEST("fluent setters") {
    Config c;
    c.workers(4)
        .queueCapacity(16)
        .onOverflow(OverflowPolicy::Reject)
        .onShutdown(ShutdownPolicy::Discard)
        .pinTo({1, 3})
        .threadName("io")
        .logLevel(LogLevel::Debug)
        .logSink([](LogLevel, const std::string&) {});
    TP_CHECK_EQ(c.num_workers, 4u);
    TP_CHECK_EQ(c.queue_capacity, 16u);
    TP_CHECK(c.overflow_policy == OverflowPolicy::Reject);
    TP_CHECK(c.shutdown_policy == ShutdownPolicy::Discard);
    TP_CHECK(c.affinity == AffinityMode::List);
    TP_CHECK(c.cpu_list == (std::vector<unsigned>{1, 3}));
    TP_CHECK_EQ(c.thread_name, std::string("io"));
    TP_CHECK(c.log_level == LogLevel::Debug);
    TP_CHECK(static_cast<bool>(c.log_sink));
    c.pinRoundRobin();
    TP_CHECK(c.affinity == AffinityMode::RoundRobin);
    TP_CHECK(c.cpu_list.empty());
    c.pinTo({});
    TP_CHECK(c.affinity == AffinityMode::None);
}

TP_TEST("validate failures") {
    Config c;
    c.workers(4096);
    c.validate();
    c.workers(4097);
    TP_CHECK_THROWS(c.validate(), ConfigError);

    Config list;
    list.affinity = AffinityMode::List;
    TP_CHECK_THROWS(list.validate(), ConfigError);
    list.cpu_list = {0};
    list.validate();

    Config name;
    name.thread_name.clear();
    TP_CHECK_THROWS(name.validate(), ConfigError);
}

TP_TEST("fromString full file") {
    const Config c = Config::fromString(
        "# leading comment\n"
        "; another comment\n"
        "\n"
        "[thread_pool]\n"
        "  num_workers     = 4   # trailing comment\n"
        "QUEUE_CAPACITY=128;trailing\n"
        "\toverflow_policy = Caller_Runs\r\n"
        "shutdown_policy = discard\n"
        "[other]\n"
        "cpu_affinity    = 0, 1 ,2\n"
        "thread_name     = worker pool\n"
        "log_level       = INFO\n");
    TP_CHECK_EQ(c.num_workers, 4u);
    TP_CHECK_EQ(c.queue_capacity, 128u);
    TP_CHECK(c.overflow_policy == OverflowPolicy::CallerRuns);
    TP_CHECK(c.shutdown_policy == ShutdownPolicy::Discard);
    TP_CHECK(c.affinity == AffinityMode::List);
    TP_CHECK(c.cpu_list == (std::vector<unsigned>{0, 1, 2}));
    TP_CHECK_EQ(c.thread_name, std::string("worker pool"));
    TP_CHECK(c.log_level == LogLevel::Info);
    TP_CHECK(c.warnings.empty());
}

TP_TEST("fromString empty and comment-only") {
    const Config a = Config::fromString("");
    TP_CHECK_EQ(a.num_workers, 0u);
    const Config b = Config::fromString("# nothing\n   \n;\n[s]\n");
    TP_CHECK_EQ(b.thread_name, std::string("tp-worker"));
    const Config bom = Config::fromString("\xEF\xBB\xBFnum_workers = 2\n");
    TP_CHECK_EQ(bom.num_workers, 2u);
}

TP_TEST("legacy format") {
    const Config c = Config::fromString("NUMS_OF_TASK_PUSHER 1\nNUMS_OF_WORKER 3", "old.txt");
    TP_CHECK_EQ(c.num_workers, 3u);
    TP_CHECK_EQ(c.warnings.size(), 2u);
    if (c.warnings.size() == 2) {
        TP_CHECK(contains(c.warnings[0], "old.txt:1:"));
        TP_CHECK(contains(c.warnings[0], "NUMS_OF_TASK_PUSHER"));
        TP_CHECK(contains(c.warnings[1], "old.txt:2:"));
        TP_CHECK(contains(c.warnings[1], "deprecated"));
    }
    // Legacy keys also work with '=' and are case-insensitive.
    const Config d = Config::fromString("nums_of_worker = 5\n");
    TP_CHECK_EQ(d.num_workers, 5u);
    TP_CHECK_EQ(d.warnings.size(), 1u);

    TP_CHECK_THROWS(Config::fromString("NUMS_OF_WORKER abc\n"), ConfigError);
    TP_CHECK_THROWS(Config::fromString("NUMS_OF_WORKER\n"), ConfigError);
}

TP_TEST("whitespace form only for legacy keys") {
    const std::string msg = errorOf([] { Config::fromString("num_workers 4\n", "f.ini"); });
    TP_CHECK(contains(msg, "f.ini:1:"));
    TP_CHECK(contains(msg, "'key = value'"));
    TP_CHECK_THROWS(Config::fromString("bogus 4\n"), ConfigError);
    TP_CHECK_THROWS(Config::fromString("justaword\n"), ConfigError);
}

TP_TEST("error messages carry source:line") {
    const std::string unknown =
        errorOf([] { Config::fromString("# c\n\n\n\n\n\nnum_wrkers = 4\n", "threadpool.ini"); });
    TP_CHECK_EQ(unknown, std::string("threadpool.ini:7: unknown key 'num_wrkers'"));

    const std::string bad = errorOf([] { Config::fromString("[x]\nnum_workers = abc\n", "threadpool.ini"); });
    TP_CHECK_EQ(bad, std::string("threadpool.ini:2: invalid value 'abc' for num_workers "
                                 "(expected a non-negative integer)"));

    TP_CHECK(contains(errorOf([] { Config::fromString("a\n[broken\n"); }), "<string>:1:"));
    TP_CHECK(contains(errorOf([] { Config::fromString("[broken\n"); }), "section"));
    TP_CHECK(contains(errorOf([] { Config::fromString("= 4\n"); }), "<string>:1:"));
    TP_CHECK(contains(errorOf([] { Config::fromString("log_level = loud\n", "x"); }), "x:1: invalid value 'loud'"));
}

TP_TEST("fromFile") {
    TempFile f("num_workers = 6\ncpu_affinity = round_robin\n");
    const Config c = Config::fromFile(f.str());
    TP_CHECK_EQ(c.num_workers, 6u);
    TP_CHECK(c.affinity == AffinityMode::RoundRobin);

    TempFile bad("\nqueue_capacity = -5\n", "tp_test_config_bad.ini");
    const std::string msg = errorOf([&] { Config::fromFile(bad.str()); });
    TP_CHECK(contains(msg, bad.str() + ":2:"));
}

TP_TEST("fromFile missing throws") {
    const auto missing = std::filesystem::temp_directory_path() / "tp_definitely_missing_config.ini";
    TP_CHECK_THROWS(Config::fromFile(missing.string()), ConfigError);
    TP_CHECK_THROWS(Config::load(missing.string()), ConfigError);
}

TP_TEST("load with nothing gives defaults") {
    clearEnv();
    const Config c = Config::load("");
    TP_CHECK_EQ(c.num_workers, 0u);
    TP_CHECK_EQ(c.thread_name, std::string("tp-worker"));
    TP_CHECK(c.overflow_policy == OverflowPolicy::Block);
}

TP_TEST("applyEnv overrides") {
    clearEnv();
    {
        EnvGuard a("TP_NUM_WORKERS", "12");
        EnvGuard b("TP_CPU_AFFINITY", "1,3");
        EnvGuard d("TP_OVERFLOW_POLICY", "reject");
        EnvGuard e("TP_LOG_LEVEL", "debug");
        EnvGuard f("TP_THREAD_NAME", "env-name");
        EnvGuard g("TP_QUEUE_CAPACITY", "64");
        EnvGuard h("TP_SHUTDOWN_POLICY", "discard");
        Config c;
        c.workers(2).applyEnv();
        TP_CHECK_EQ(c.num_workers, 12u);
        TP_CHECK(c.affinity == AffinityMode::List);
        TP_CHECK(c.cpu_list == (std::vector<unsigned>{1, 3}));
        TP_CHECK(c.overflow_policy == OverflowPolicy::Reject);
        TP_CHECK(c.log_level == LogLevel::Debug);
        TP_CHECK_EQ(c.thread_name, std::string("env-name"));
        TP_CHECK_EQ(c.queue_capacity, 64u);
        TP_CHECK(c.shutdown_policy == ShutdownPolicy::Discard);
    }
    Config untouched;
    untouched.workers(2).applyEnv();  // guards removed the variables
    TP_CHECK_EQ(untouched.num_workers, 2u);
}

TP_TEST("applyEnv error names the variable") {
    clearEnv();
    EnvGuard a("TP_NUM_WORKERS", "lots");
    const std::string msg = errorOf([] { Config().applyEnv(); });
    TP_CHECK(contains(msg, "TP_NUM_WORKERS"));
    TP_CHECK(contains(msg, "invalid value 'lots'"));
}

TP_TEST("load: env beats file, TP_CONFIG_FILE used") {
    clearEnv();
    TempFile f("num_workers = 3\nthread_name = from-file\nlog_level = error\n", "tp_test_config_load.ini");
    {
        EnvGuard file("TP_CONFIG_FILE", f.str().c_str());
        EnvGuard workers("TP_NUM_WORKERS", "9");
        const Config c = Config::load();
        TP_CHECK_EQ(c.num_workers, 9u);                           // env wins
        TP_CHECK_EQ(c.thread_name, std::string("from-file"));     // file applies
        TP_CHECK(c.log_level == LogLevel::Error);
    }
    {
        const Config c = Config::load(f.str());  // explicit path
        TP_CHECK_EQ(c.num_workers, 3u);
    }
    {
        EnvGuard file("TP_CONFIG_FILE", "tp_no_such_file_anywhere.ini");
        TP_CHECK_THROWS(Config::load(), ConfigError);
    }
}

TP_TEST("load validates") {
    clearEnv();
    TempFile f("num_workers = 5000\n", "tp_test_config_big.ini");
    TP_CHECK_THROWS(Config::load(f.str()), ConfigError);
}

TP_TEST("toString round-trips through the parser") {
    for (auto p : {OverflowPolicy::Block, OverflowPolicy::Reject, OverflowPolicy::CallerRuns}) {
        Config c;
        c.set("overflow_policy", tp::toString(p));
        TP_CHECK(c.overflow_policy == p);
    }
    for (auto p : {ShutdownPolicy::Drain, ShutdownPolicy::Discard}) {
        Config c;
        c.set("shutdown_policy", tp::toString(p));
        TP_CHECK(c.shutdown_policy == p);
    }
    for (auto l : {LogLevel::Off, LogLevel::Error, LogLevel::Warn, LogLevel::Info, LogLevel::Debug}) {
        Config c;
        c.set("log_level", tp::toString(l));
        TP_CHECK(c.log_level == l);
    }
    TP_CHECK_EQ(std::string(tp::toString(AffinityMode::None)), std::string("none"));
    TP_CHECK_EQ(std::string(tp::toString(AffinityMode::RoundRobin)), std::string("round_robin"));
    TP_CHECK_EQ(std::string(tp::toString(AffinityMode::List)), std::string("list"));
    TP_CHECK_EQ(std::string(tp::toString(OverflowPolicy::CallerRuns)), std::string("caller_runs"));
}

TP_TEST_MAIN
