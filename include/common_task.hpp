#ifndef COMMON_TASK_HPP
#define COMMON_TASK_HPP

#include <functional>
struct TaskData {
    std::function<void(int, int)> callBackFunc;
    int value_1;
    int value_2;
};

class CommonTask {
public:
    /**
     * @brief Returns a function that adds two integers.
     * 
     * @return A function that takes two integers and returns their sum.
     */
    static std::function<void(int, int)> getAddTask();

    /**
     * @brief Returns a function that subtracts two integers.
     * 
     * @return A function that takes two integers and returns their difference.
     */
    static std::function<void(int, int)> getSubtractTask();

    /**
     * @brief Returns a function that multiplies two integers.
     * 
     * @return A function that takes two integers and returns their product.
     */
    static std::function<void(int, int)> getMultiplyTask();
    
    /**
     * @brief Returns a function that divides two integers.
     * 
     * @return A function that takes two integers and returns their quotient.
     */
    static std::function<void(int, int)> getDivideTask();

    /**
     * @brief Returns a function that generates the Fibonacci sequence.
     * 
     * @return A function that takes an integer n and prints the Fibonacci sequence
     * up to n.
     */
    static std::function<void(int, int)> getFibonacciTask();
};

#endif // COMMON_TASK_HPP