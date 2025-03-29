#include "common_task.hpp"
#include <random>
#include <ctime>

TaskData* CommonTask::generateRandomTask() {
    static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
    
    std::uniform_int_distribution<int> taskTypeDist(0, static_cast<int>(TaskType::Count) - 1);
    TaskType taskType = static_cast<TaskType>(taskTypeDist(rng));
    
    std::uniform_int_distribution<int> argCountDist(1, 5);
    int argCount = argCountDist(rng);
    
    std::uniform_int_distribution<int> argValueDist(1, 20);
    
    std::uniform_int_distribution<int> nonZeroArgDist(1, 20);
    
    TaskData* task = nullptr;
    
    switch (taskType) {
        case TaskType::Addition: {
            if (argCount == 1) {
                task = new TaskData(getAddTask<int>, argValueDist(rng));
            } else if (argCount == 2) {
                task = new TaskData(getAddTask<int, int>, 
                                   argValueDist(rng), argValueDist(rng));
            } else if (argCount == 3) {
                task = new TaskData(getAddTask<int, int, int>, 
                                   argValueDist(rng), argValueDist(rng), argValueDist(rng));
            } else if (argCount == 4) {
                task = new TaskData(getAddTask<int, int, int, int>, 
                                   argValueDist(rng), argValueDist(rng), 
                                   argValueDist(rng), argValueDist(rng));
            } else {
                task = new TaskData(getAddTask<int, int, int, int, int>, 
                                   argValueDist(rng), argValueDist(rng), argValueDist(rng), 
                                   argValueDist(rng), argValueDist(rng));
            }
            break;
        }
        case TaskType::Subtraction: {
            if (argCount == 1) {
                task = new TaskData(getSubtractTask<int>, argValueDist(rng));
            } else if (argCount == 2) {
                task = new TaskData(getSubtractTask<int, int>, 
                                   argValueDist(rng), argValueDist(rng));
            } else if (argCount == 3) {
                task = new TaskData(getSubtractTask<int, int, int>, 
                                   argValueDist(rng), argValueDist(rng), argValueDist(rng));
            } else if (argCount == 4) {
                task = new TaskData(getSubtractTask<int, int, int, int>, 
                                   argValueDist(rng), argValueDist(rng), 
                                   argValueDist(rng), argValueDist(rng));
            } else {
                task = new TaskData(getSubtractTask<int, int, int, int, int>, 
                                   argValueDist(rng), argValueDist(rng), argValueDist(rng), 
                                   argValueDist(rng), argValueDist(rng));
            }
            break;
        }
        case TaskType::Multiplication: {
            if (argCount == 1) {
                task = new TaskData(getMultiplyTask<int>, argValueDist(rng));
            } else if (argCount == 2) {
                task = new TaskData(getMultiplyTask<int, int>, 
                                   argValueDist(rng), argValueDist(rng));
            } else if (argCount == 3) {
                task = new TaskData(getMultiplyTask<int, int, int>, 
                                   argValueDist(rng), argValueDist(rng), argValueDist(rng));
            } else if (argCount == 4) {
                task = new TaskData(getMultiplyTask<int, int, int, int>, 
                                   argValueDist(rng), argValueDist(rng), 
                                   argValueDist(rng), argValueDist(rng));
            } else {
                task = new TaskData(getMultiplyTask<int, int, int, int, int>, 
                                   argValueDist(rng), argValueDist(rng), argValueDist(rng), 
                                   argValueDist(rng), argValueDist(rng));
            }
            break;
        }
        case TaskType::Division: {
            if (argCount == 1) {
                task = new TaskData(getDivideTask<int>, nonZeroArgDist(rng));
            } else if (argCount == 2) {
                task = new TaskData(getDivideTask<int, int>, 
                                   nonZeroArgDist(rng), nonZeroArgDist(rng));
            } else if (argCount == 3) {
                task = new TaskData(getDivideTask<int, int, int>, 
                                   nonZeroArgDist(rng), nonZeroArgDist(rng), nonZeroArgDist(rng));
            } else if (argCount == 4) {
                task = new TaskData(getDivideTask<int, int, int, int>, 
                                   nonZeroArgDist(rng), nonZeroArgDist(rng), 
                                   nonZeroArgDist(rng), nonZeroArgDist(rng));
            } else {
                task = new TaskData(getDivideTask<int, int, int, int, int>, 
                                   nonZeroArgDist(rng), nonZeroArgDist(rng), nonZeroArgDist(rng), 
                                   nonZeroArgDist(rng), nonZeroArgDist(rng));
            }
            break;
        }
        case TaskType::Fibonacci: {
            // Fibonacci only needs one argument
            task = new TaskData(getFibonacciTask<int>, argValueDist(rng));
            break;
        }
        default:
            std::cerr << "Unknown task type!" << std::endl;
            break;
    }
    return task;
}