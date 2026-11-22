#include "tp/config.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <thread>

namespace tp {

namespace {

constexpr std::size_t kMaxWorkers = 4096;

// Every key the file / set() / environment understands, in canonical form.
const char* const kKeys[] = {
    "num_workers",  "queue_capacity", "overflow_policy", "shutdown_policy",
    "cpu_affinity", "thread_name",    "log_level",
};

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n\f\v";
    const auto first = s.find_first_not_of(ws);
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(ws);
    return s.substr(first, last - first + 1);
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

[[noreturn]] void invalid(const std::string& ctx, const std::string& value, const std::string& key,
                          const char* expected) {
    throw ConfigError(ctx + "invalid value '" + value + "' for " + key + " (expected " + expected + ")");
}

// Strict unsigned parse: digits only, no sign, no trailing junk, no overflow.
bool parseUnsigned(const std::string& text, unsigned long long max, unsigned long long& out) {
    if (text.empty()) return false;
    unsigned long long v = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        const unsigned digit = static_cast<unsigned>(c - '0');
        if (v > (max - digit) / 10) return false;
        v = v * 10 + digit;
    }
    out = v;
    return true;
}

std::size_t parseSize(const std::string& ctx, const std::string& key, const std::string& value) {
    unsigned long long v = 0;
    if (!parseUnsigned(value, std::numeric_limits<std::size_t>::max(), v))
        invalid(ctx, value, key, "a non-negative integer");
    return static_cast<std::size_t>(v);
}

// "0,2, 4" -> {0, 2, 4}. Rejects empty items ("0,,2") and non-numbers.
std::vector<unsigned> parseCpuList(const std::string& ctx, const std::string& key, const std::string& value) {
    std::vector<unsigned> cpus;
    std::size_t start = 0;
    while (true) {
        const auto comma = value.find(',', start);
        const std::string item = trim(value.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        unsigned long long v = 0;
        if (!parseUnsigned(item, std::numeric_limits<unsigned>::max(), v))
            invalid(ctx, value, key, "none, round_robin or a comma-separated list of CPU indices");
        cpus.push_back(static_cast<unsigned>(v));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return cpus;
}

// Shared by set(), the file parser and applyEnv(). `ctx` is an error-message
// prefix such as "threadpool.ini:7: " (empty for direct set() calls).
void applyKey(Config& c, const std::string& rawKey, const std::string& rawValue, const std::string& ctx) {
    const std::string key = lower(trim(rawKey));
    const std::string value = trim(rawValue);
    const std::string lv = lower(value);

    if (key == "num_workers") {
        c.num_workers = parseSize(ctx, key, value);
    } else if (key == "queue_capacity") {
        c.queue_capacity = parseSize(ctx, key, value);
    } else if (key == "overflow_policy") {
        if (lv == "block") c.overflow_policy = OverflowPolicy::Block;
        else if (lv == "reject") c.overflow_policy = OverflowPolicy::Reject;
        else if (lv == "caller_runs") c.overflow_policy = OverflowPolicy::CallerRuns;
        else invalid(ctx, value, key, "block, reject or caller_runs");
    } else if (key == "shutdown_policy") {
        if (lv == "drain") c.shutdown_policy = ShutdownPolicy::Drain;
        else if (lv == "discard") c.shutdown_policy = ShutdownPolicy::Discard;
        else invalid(ctx, value, key, "drain or discard");
    } else if (key == "cpu_affinity") {
        if (lv == "none") {
            c.affinity = AffinityMode::None;
            c.cpu_list.clear();
        } else if (lv == "round_robin") {
            c.affinity = AffinityMode::RoundRobin;
            c.cpu_list.clear();
        } else {
            c.cpu_list = parseCpuList(ctx, key, value);
            c.affinity = AffinityMode::List;
        }
    } else if (key == "thread_name") {
        if (value.empty()) invalid(ctx, value, key, "a non-empty name");
        c.thread_name = value;
    } else if (key == "log_level") {
        if (lv == "off") c.log_level = LogLevel::Off;
        else if (lv == "error") c.log_level = LogLevel::Error;
        else if (lv == "warn") c.log_level = LogLevel::Warn;
        else if (lv == "info") c.log_level = LogLevel::Info;
        else if (lv == "debug") c.log_level = LogLevel::Debug;
        else invalid(ctx, value, key, "off, error, warn, info or debug");
    } else if (key == "nums_of_worker") {  // legacy format
        c.num_workers = parseSize(ctx, "NUMS_OF_WORKER", value);
        c.warnings.push_back(ctx + "'NUMS_OF_WORKER' is deprecated, use 'num_workers' instead");
    } else if (key == "nums_of_task_pusher") {  // legacy, no longer meaningful
        c.warnings.push_back(ctx + "'NUMS_OF_TASK_PUSHER' is obsolete and ignored");
    } else if (key == "worker_count") {  // ConfigLoader-era names
        c.num_workers = parseSize(ctx, key, value);
        c.warnings.push_back(ctx + "'worker_count' is deprecated, use 'num_workers' instead");
    } else if (key == "queue_size") {
        c.queue_capacity = parseSize(ctx, key, value);
        c.warnings.push_back(ctx + "'queue_size' is deprecated, use 'queue_capacity' instead");
    } else if (key == "task_pusher_count" || key == "dynamic_sizing" || key == "lifecycle_timeout" ||
               key == "enable_monitoring") {
        c.warnings.push_back(ctx + "'" + key + "' is not supported by tp::ThreadPool and is ignored");
    } else {
        throw ConfigError(ctx + "unknown key '" + trim(rawKey) + "'");
    }
}

bool isLegacyKey(const std::string& key) {
    const std::string k = lower(key);
    return k == "nums_of_worker" || k == "nums_of_task_pusher";
}

// Returns false when the variable is unset or empty (Windows cannot store
// empty environment variables, so treat both the same everywhere).
bool getEnv(const std::string& name, std::string& out) {
#ifdef _MSC_VER
    char* buf = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&buf, &len, name.c_str()) != 0 || buf == nullptr) return false;
    out = buf;
    std::free(buf);
#else
    const char* v = std::getenv(name.c_str());
    if (v == nullptr) return false;
    out = v;
#endif
    return !out.empty();
}

}  // namespace

Config& Config::set(const std::string& key, const std::string& value) {
    applyKey(*this, key, value, "");
    return *this;
}

Config& Config::applyEnv() {
    for (const char* key : kKeys) {
        const std::string var = "TP_" + upper(key);
        std::string value;
        if (getEnv(var, value)) applyKey(*this, key, value, "environment variable " + var + ": ");
    }
    return *this;
}

std::size_t Config::resolvedWorkers() const {
    if (num_workers != 0) return num_workers;
    const unsigned hw = std::thread::hardware_concurrency();
    return hw == 0 ? 1 : hw;
}

void Config::validate() const {
    if (num_workers > kMaxWorkers)
        throw ConfigError("num_workers = " + std::to_string(num_workers) + " exceeds the limit of " +
                          std::to_string(kMaxWorkers));
    if (affinity == AffinityMode::List && cpu_list.empty())
        throw ConfigError("cpu_affinity is a CPU list but cpu_list is empty");
    if (thread_name.empty()) throw ConfigError("thread_name must not be empty");
}

Config Config::fromString(const std::string& text, const std::string& source) {
    Config c;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        const std::string ctx = source + ":" + std::to_string(lineNo) + ": ";

        // Skip a UTF-8 byte order mark (common in files saved by Windows editors).
        if (lineNo == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);

        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) line.erase(comment);
        line = trim(line);
        if (line.empty()) continue;

        if (line.front() == '[') {
            if (line.back() != ']') throw ConfigError(ctx + "malformed section header '" + line + "'");
            continue;  // sections are accepted and ignored
        }

        const auto eq = line.find('=');
        if (eq != std::string::npos) {
            const std::string key = trim(line.substr(0, eq));
            if (key.empty()) throw ConfigError(ctx + "missing key before '='");
            applyKey(c, key, line.substr(eq + 1), ctx);
            continue;
        }

        // No '=': only the legacy "KEY value" form is allowed.
        const auto ws = line.find_first_of(" \t");
        const std::string key = line.substr(0, ws);
        if (!isLegacyKey(key)) {
            // Distinguish "num_workers 4" (missing '=') from a typo'd key.
            const std::string lk = lower(key);
            for (const char* k : kKeys)
                if (lk == k) throw ConfigError(ctx + "expected 'key = value' for " + lk);
            throw ConfigError(ctx + "unknown key '" + key + "'");
        }
        applyKey(c, key, ws == std::string::npos ? std::string() : line.substr(ws), ctx);
    }
    return c;
}

Config Config::fromFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw ConfigError("cannot open config file '" + path + "'");
    std::ostringstream ss;
    ss << in.rdbuf();
    return fromString(ss.str(), path);
}

Config Config::load(const std::string& path) {
    Config c;
    std::string file = path;
    if (file.empty()) getEnv("TP_CONFIG_FILE", file);
    if (!file.empty()) c = fromFile(file);
    c.applyEnv();
    c.validate();
    return c;
}

const char* toString(OverflowPolicy p) {
    switch (p) {
        case OverflowPolicy::Block: return "block";
        case OverflowPolicy::Reject: return "reject";
        case OverflowPolicy::CallerRuns: return "caller_runs";
    }
    return "unknown";
}

const char* toString(ShutdownPolicy p) {
    switch (p) {
        case ShutdownPolicy::Drain: return "drain";
        case ShutdownPolicy::Discard: return "discard";
    }
    return "unknown";
}

const char* toString(AffinityMode m) {
    switch (m) {
        case AffinityMode::None: return "none";
        case AffinityMode::RoundRobin: return "round_robin";
        case AffinityMode::List: return "list";
    }
    return "unknown";
}

const char* toString(LogLevel l) {
    switch (l) {
        case LogLevel::Off: return "off";
        case LogLevel::Error: return "error";
        case LogLevel::Warn: return "warn";
        case LogLevel::Info: return "info";
        case LogLevel::Debug: return "debug";
    }
    return "unknown";
}

}  // namespace tp
