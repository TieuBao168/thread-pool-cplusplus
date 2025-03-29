#include "thread_pool.hpp"
#include "common_task.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <unistd.h>

int main() {
    ThreadPool pool;
    unsigned int totalThreads = std::thread::hardware_concurrency();
    long physicalCores = sysconf(_SC_NPROCESSORS_ONLN);
    std::cout << "#### CPU INFO #####" << "\n";
    std::cout << "Total Threads: " << totalThreads << "\n";
    std::cout << "Physical Cores: " << physicalCores << "\n";
    std::cout << "Threads per Core: " << (totalThreads / physicalCores) << "\n";
    
    std::cout << "\n##### Initializing thread pool #####" << "\n";
    if (!pool.initialize("threadpool_config.txt")) {
        std::cerr << "Failed to initialize thread pool" << "\n";
        return 1;
    }
    
    std::cout << "Thread pool initialized with: [" 
              << pool.getNumTaskPushers() << " task pushers] and [" 
              << pool.getNumWorkers() << " workers]" << "\n";
    
    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    std::cout << "Main thread exiting" << "\n";
    return 0;
}