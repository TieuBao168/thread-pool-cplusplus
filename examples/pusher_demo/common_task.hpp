#ifndef DEMO_COMMON_TASK_HPP
#define DEMO_COMMON_TASK_HPP

// Arithmetic demo tasks used by pusher_demo (ported from the legacy CommonTask).
//
// Every task builds its whole output line first and prints it with a single
// call to printLine(), so lines from different workers never interleave.

#include <functional>
#include <random>
#include <sstream>
#include <string>

namespace demo {

/// Writes one complete line to stdout; safe to call from any thread.
void printLine(const std::string& line);

template <typename T, typename... Rest>
void addTask(T first, Rest... rest) {
    std::ostringstream out;
    T sum = first;
    out << "Adding: " << first;
    ((out << " + " << rest, sum += rest), ...);
    out << " = " << sum;
    printLine(out.str());
}

template <typename T, typename... Rest>
void subtractTask(T first, Rest... rest) {
    std::ostringstream out;
    T difference = first;
    out << "Subtracting: " << first;
    ((out << " - " << rest, difference -= rest), ...);
    out << " = " << difference;
    printLine(out.str());
}

template <typename T, typename... Rest>
void multiplyTask(T first, Rest... rest) {
    std::ostringstream out;
    T product = first;
    out << "Multiplying: " << first;
    ((out << " * " << rest, product *= rest), ...);
    out << " = " << product;
    printLine(out.str());
}

template <typename T, typename... Rest>
void divideTask(T first, Rest... rest) {
    std::ostringstream out;
    T quotient = first;
    bool divByZero = false;
    out << "Dividing: " << first;
    [[maybe_unused]] auto step = [&](T divisor) {
        out << " / " << divisor;
        if (divisor == T{}) {
            divByZero = true;
        } else if (!divByZero) {
            quotient /= divisor;
        }
    };
    (step(rest), ...);
    if (divByZero) {
        out << " = <error: division by zero>";
    } else {
        out << " = " << quotient;
    }
    printLine(out.str());
}

template <typename T>
void fibonacciTask(T terms) {
    std::ostringstream out;
    out << "Fibonacci sequence up to " << terms << " terms:";
    T a = 0, b = 1;
    for (T i = 0; i < terms; ++i) {
        out << ' ' << a;
        T next = a + b;
        a = b;
        b = next;
    }
    printLine(out.str());
}

/**
 * Builds a random arithmetic task (add / subtract / multiply / divide with
 * 1-5 arguments, or a Fibonacci sequence) ready to hand to ThreadPool::post().
 * Uses only the caller's generator, so each producer thread can own one.
 */
std::function<void()> generateRandomTask(std::mt19937& rng);

}  // namespace demo

#endif  // DEMO_COMMON_TASK_HPP
