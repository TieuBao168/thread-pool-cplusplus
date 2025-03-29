#ifndef COMMON_TASK_HPP
#define COMMON_TASK_HPP

#include <iostream>
#include <functional>
#include <tuple>

struct TaskData {
    std::function<void()> callBackFunc;

    template<typename Func, typename... Args>
    TaskData(Func&& func, Args&&... args) {
        callBackFunc = [f = std::forward<Func>(func), tup = std::make_tuple(std::forward<Args>(args)...)]() {
            std::apply(f, tup);
        };
    }
};

enum class TaskType {
    Addition,
    Subtraction,
    Multiplication,
    Division,
    Fibonacci,
    Count
};

class CommonTask {
public:
    template<typename T, typename... Args>
    static void getAddTask(T first, Args... rest) {
        std::cout << "Adding: ";
        std::cout << first;
        T sum = first;
        
        (void)std::initializer_list<int>{([&sum, &rest]() {
            std::cout << " + " << rest;
            sum += rest;
            return 0;
        }())...};
        
        std::cout << " = " << sum << '\n';
    }

    template<typename T, typename... Args>
    static void getSubtractTask(T first, Args... rest) {
        std::cout << "Subtracting: ";
        std::cout << first;
        T difference = first;
        
        (void)std::initializer_list<int>{([&difference, &rest]() {
            std::cout << " - " << rest;
            difference -= rest;
            return 0;
        }())...};
        
        std::cout << " = " << difference << '\n';
    }

    template<typename T, typename... Args>
    static void getMultiplyTask(T first, Args... rest) {
        std::cout << "Multiplying: ";
        std::cout << first;
        T product = first;
        
        (void)std::initializer_list<int>{([&product, &rest]() {
            std::cout << " * " << rest;
            product *= rest;
            return 0;
        }())...};
        
        std::cout << " = " << product << '\n';
    }

    template<typename T, typename... Args>
    static void getDivideTask(T first, Args... rest) {
        std::cout << "Dividing: ";
        std::cout << first;
        T quotient = first;
        bool error = false;
        
        (void)std::initializer_list<int>{([&quotient, &rest, &error]() {
            std::cout << " / " << rest;
            if (rest != 0) {
                quotient /= rest;
            } else {
                std::cout << " (Error: Division by zero)";
                error = true;
            }
            return 0;
        }())...};
        
        if (!error) {
            std::cout << " = " << quotient << '\n';
        } else {
            std::cout << "\nCannot complete calculation due to division by zero.\n";
        }
    }

    template<typename T>
    static void getFibonacciTask(T n) {
        T a = 0, b = 1;
        std::cout << "Fibonacci sequence up to " << n << " terms: ";
        for (T i = 0; i < n; i++) {
            std::cout << a << " ";
            T temp = a;
            a = b;
            b = temp + b;
        }
        std::cout << "\n";
    }

    /**
    * Generates a random task based on the given distribution.
    *
    * @return A dynamically allocated TaskData object with a random task type and
    *         arguments.
    */
    static TaskData* generateRandomTask();
};

#endif // COMMON_TASK_HPP