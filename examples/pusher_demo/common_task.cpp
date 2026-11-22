#include "common_task.hpp"

#include <array>
#include <iostream>
#include <mutex>

namespace demo {

void printLine(const std::string& line) {
    static std::mutex outputMutex;
    std::lock_guard<std::mutex> lock(outputMutex);
    std::cout << line << '\n';
}

namespace {

enum class TaskType { Addition, Subtraction, Multiplication, Division, Fibonacci, Count };

constexpr std::size_t kMaxArgs = 5;

/// Binds `op` (a generic callable) to the first `count` values of `args`.
template <typename Op>
std::function<void()> bindArgs(Op op, const std::array<int, kMaxArgs>& a, int count) {
    switch (count) {
        case 1: return [op, a] { op(a[0]); };
        case 2: return [op, a] { op(a[0], a[1]); };
        case 3: return [op, a] { op(a[0], a[1], a[2]); };
        case 4: return [op, a] { op(a[0], a[1], a[2], a[3]); };
        default: return [op, a] { op(a[0], a[1], a[2], a[3], a[4]); };
    }
}

}  // namespace

std::function<void()> generateRandomTask(std::mt19937& rng) {
    std::uniform_int_distribution<int> typeDist(0, static_cast<int>(TaskType::Count) - 1);
    std::uniform_int_distribution<int> countDist(1, static_cast<int>(kMaxArgs));
    std::uniform_int_distribution<int> valueDist(1, 20);  // never 0: safe divisors

    const auto type = static_cast<TaskType>(typeDist(rng));
    const int count = countDist(rng);
    std::array<int, kMaxArgs> args{};
    for (int& v : args) v = valueDist(rng);

    switch (type) {
        case TaskType::Addition:
            return bindArgs([](auto... xs) { addTask(xs...); }, args, count);
        case TaskType::Subtraction:
            return bindArgs([](auto... xs) { subtractTask(xs...); }, args, count);
        case TaskType::Multiplication:
            return bindArgs([](auto... xs) { multiplyTask(xs...); }, args, count);
        case TaskType::Division:
            // Divide as double so the results are not all truncated to 0.
            return bindArgs([](auto... xs) { divideTask(static_cast<double>(xs)...); }, args,
                            count);
        case TaskType::Fibonacci:
        case TaskType::Count:
            break;
    }
    const int terms = args[0];
    return [terms] { fibonacciTask(terms); };
}

}  // namespace demo
