#include "common_task.hpp"
#include <iostream>

std::function<void(int, int)> CommonTask::getAddTask() {
    return [](int a, int b) {
        std::cout << "Addition: " << a << " + " << b << " = " << (a + b) << std::endl;
    };
}

std::function<void(int, int)> CommonTask::getSubtractTask() {
    return [](int a, int b) {
        std::cout << "Subtraction: " << a << " - " << b << " = " << (a - b) << std::endl;
    };
}

std::function<void(int, int)> CommonTask::getMultiplyTask() {
    return [](int a, int b) {
        std::cout << "Multiplication: " << a << " * " << b << " = " << (a * b) << std::endl;
    };
}

std::function<void(int, int)> CommonTask::getDivideTask() {
    return [](int a, int b) {
        if (b == 0) {
            std::cout << "Division by zero error" << std::endl;
            return;
        }
        std::cout << "Division: " << a << " / " << b << " = " << (a / b) << std::endl;
    };
}

std::function<void(int, int)> CommonTask::getFibonacciTask() {
    return [](int n, int unused) {
        (void)unused; // Suppress unused parameter warning
        if (n <= 0) {
            std::cout << "Fibonacci requires a positive integer" << std::endl;
            return;
        }
        
        int a = 0, b = 1;
        std::cout << "Fibonacci sequence for n=" << n << ": ";
        for (int i = 0; i < n; ++i) {
            std::cout << a << " ";
            int temp = a;
            a = b;
            b = temp + b;
        }
        std::cout << std::endl;
    };
}