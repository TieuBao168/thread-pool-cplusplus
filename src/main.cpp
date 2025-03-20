#include "thread_pool.hpp"
#include "common_task.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    ThreadPool pool;
    
    if (!pool.initialize("threadpool_config.txt")) {
        std::cerr << "Failed to initialize thread pool" << std::endl;
        return 1;
    }
    
    std::cout << "Thread pool initialized with: [" 
              << pool.getNumTaskPushers() << " task pushers] and [" 
              << pool.getNumWorkers() << " workers]" << std::endl;
    
    TaskData* task1 = new TaskData{CommonTask::getAddTask(), 10, 20};
    TaskData* task2 = new TaskData{CommonTask::getSubtractTask(), 50, 30};
    TaskData* task3 = new TaskData{CommonTask::getMultiplyTask(), 5, 7};
    TaskData* task4 = new TaskData{CommonTask::getDivideTask(), 100, 4};
    TaskData* task5 = new TaskData{CommonTask::getFibonacciTask(), 10, 0};
    
    pool.pushTask(static_cast<void*>(task1));
    pool.pushTask(static_cast<void*>(task2));
    pool.pushTask(static_cast<void*>(task3));
    pool.pushTask(static_cast<void*>(task4));
    pool.pushTask(static_cast<void*>(task5));
    
    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    std::cout << "Main thread exiting" << std::endl;
    return 0;
}