#include <iostream>
#include <chrono>
#include <thread>
#include "utils/ConfigLoader.hpp"
#include "tinythpool/ThreadPool.hpp"

int main() {
    ConfigLoader& config = ConfigLoader::getInstance();
    if (!config.initialize("config/thread_pool_config.txt")) {
        std::cerr << "Failed to load configuration file!" << std::endl;
        return 1;
    }

    ThreadPool pool;

    std::cout << "#### CPU INFO #####" << "\n";
    std::cout << "Hardware threads: " << std::thread::hardware_concurrency() << "\n";
    
    std::cout << "\n##### Initializing thread pool #####" << "\n";
    if (!pool.initialize()) {
        std::cerr << "Failed to initialize thread pool" << "\n";
        return 1;
    }
    
    std::cout << "Thread pool initialized with: [" 
              << pool.getNumTaskPushers() << " task pushers] and [" 
              << pool.getNumWorkers() << " workers]" << "\n";
    
    // Wait for tasks to complete
    int timeout = config.getValue<int>("lifecycle_timeout", 60);
    std::cout << "Will close thread pool after " << timeout << " seconds" << "\n";
    std::this_thread::sleep_for(std::chrono::seconds(timeout));
    
    std::cout << "Main thread exiting" << "\n";
    return 0;
}