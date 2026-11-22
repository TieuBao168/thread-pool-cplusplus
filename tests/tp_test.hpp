// Minimal dependency-free test harness.
//
//   TP_TEST(name) { TP_CHECK(cond); TP_CHECK_EQ(a, b); TP_CHECK_THROWS(expr, Type); }
//
// Each test file is its own executable; define TP_TEST_MAIN in exactly one
// translation unit of it (or just include this header in a single .cpp).
// Exit code is the number of failed tests (0 = success).
#ifndef TP_TEST_HPP
#define TP_TEST_HPP

#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace tp_test {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failures() {
    static int n = 0;
    return n;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

inline void fail(const char* file, int line, const std::string& what) {
    ++failures();
    std::cerr << "  " << file << ":" << line << ": FAILED: " << what << "\n";
}

// Run all tests, or only those whose name contains argv[1].
inline int run(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int failedCases = 0, ran = 0;
    for (auto& c : registry()) {
        if (filter && !std::strstr(c.name, filter)) continue;
        ++ran;
        int before = failures();
        std::cout << "[ RUN  ] " << c.name << std::endl;
        try {
            c.fn();
        } catch (const std::exception& e) {
            fail(__FILE__, __LINE__, std::string("unexpected exception: ") + e.what());
        } catch (...) {
            fail(__FILE__, __LINE__, "unexpected non-std exception");
        }
        bool ok = failures() == before;
        if (!ok) ++failedCases;
        std::cout << (ok ? "[  OK  ] " : "[ FAIL ] ") << c.name << std::endl;
    }
    std::cout << ran - failedCases << "/" << ran << " tests passed" << std::endl;
    return failedCases;
}

}  // namespace tp_test

#define TP_CAT2(a, b) a##b
#define TP_CAT(a, b) TP_CAT2(a, b)

#define TP_TEST(name)                                                              \
    static void TP_CAT(tp_test_fn_, __LINE__)();                                   \
    static ::tp_test::Registrar TP_CAT(tp_test_reg_, __LINE__)(name, &TP_CAT(tp_test_fn_, __LINE__)); \
    static void TP_CAT(tp_test_fn_, __LINE__)()

#define TP_CHECK(cond)                                                             \
    do {                                                                           \
        if (!(cond)) ::tp_test::fail(__FILE__, __LINE__, #cond);                   \
    } while (0)

#define TP_CHECK_EQ(a, b)                                                          \
    do {                                                                           \
        auto&& tp_a_ = (a);                                                        \
        auto&& tp_b_ = (b);                                                        \
        if (!(tp_a_ == tp_b_)) {                                                   \
            std::ostringstream tp_os_;                                             \
            tp_os_ << #a " == " #b " (" << tp_a_ << " vs " << tp_b_ << ")";        \
            ::tp_test::fail(__FILE__, __LINE__, tp_os_.str());                     \
        }                                                                          \
    } while (0)

#define TP_CHECK_THROWS(expr, Type)                                                \
    do {                                                                           \
        bool tp_thrown_ = false;                                                   \
        try {                                                                      \
            (void)(expr);                                                          \
        } catch (const Type&) {                                                    \
            tp_thrown_ = true;                                                     \
        } catch (...) {                                                            \
        }                                                                          \
        if (!tp_thrown_) ::tp_test::fail(__FILE__, __LINE__, #expr " did not throw " #Type); \
    } while (0)

#define TP_TEST_MAIN                                                               \
    int main(int argc, char** argv) { return ::tp_test::run(argc, argv); }

#endif  // TP_TEST_HPP
