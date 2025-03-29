#include "thread_pool.hpp"
#include "common_task.hpp"
#include <iostream>
#include <thread>
#include <sstream>

ThreadPool::ThreadPool() : 
    numTaskPushers(0), 
    numWorkers(0), 
    taskPusherThreads(), 
    workerThreads(),
    running(false),
    head(nullptr), 
    tail(nullptr) {}

ThreadPool::~ThreadPool() {
    running = false;
    
    listCondition.notify_all();
    
    for (auto& thread : taskPusherThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(listMutex);
        while (head != nullptr) {
            TaskNode* temp = head;
            head = head->next;
            delete static_cast<TaskData*>(temp->data);
            delete temp;
        }
        tail = nullptr;
    }
}

bool ThreadPool::initialize(const std::string& configFile) {
    std::ifstream config(configFile);
    if (!config.is_open()) {
        std::cerr << "Failed to open config file: " << configFile << "\n";
        return false;
    }
    
    std::string line, key;
    while (std::getline(config, line)) {
        std::istringstream iss(line);
        if (iss >> key) {
            if (key == "NUMS_OF_TASK_PUSHER") {
                iss >> numTaskPushers;
            } else if (key == "NUMS_OF_WORKER") {
                iss >> numWorkers;
            }
        }
    }
    
    if (numTaskPushers <= 0 || numWorkers <= 0) {
        std::cerr << "Invalid configuration values" << "\n";
        return false;
    }
    
    running = true;
    
    // Create task pusher threads
    for (int i = 1; i <= numTaskPushers; ++i) {
        taskPusherThreads.emplace_back(&ThreadPool::taskPusherThread, this, i);
    }
    
    // Create worker threads
    for (int i = 1; i <= numWorkers; ++i) {
        workerThreads.emplace_back(&ThreadPool::workerThread, this, i);
    }
    
    return true;
}

void ThreadPool::pushTask(void* data) {
    // Create a new node for the linked list
    TaskNode* newNode = new TaskNode(data);
    
    {
        std::lock_guard<std::mutex> lock(listMutex);
        if (head == nullptr) {
            head = newNode;
            tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }
    }
    
    listCondition.notify_one();
}

void ThreadPool::taskPusherThread(int id) {
    // Set thread affinity to core 0
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    std::cout << "Task pusher thread " << id << " started on core 0" << "\n";

    /* Create tasks example
    TaskData* task[5] = {nullptr};
    task[0] = new TaskData{CommonTask::getAddTask<int, float>, 50, 30};
    task[1] = new TaskData{CommonTask::getSubtractTask<int, float>, 50, 30};
    task[2] = new TaskData{CommonTask::getMultiplyTask<int, int>, 5, 7};
    task[3] = new TaskData{CommonTask::getDivideTask<int, int>, 100, 0};
    task[4] = new TaskData{CommonTask::getFibonacciTask<int>, 10};
    */

    // generate tasks based on some data source or trigger.
    while (running) {
        // Sleep to simulate work
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        this->pushTask(static_cast<void*>(CommonTask::generateRandomTask()));
        std::cout << "Task pushed by task pusher thread " << id << "\n";
    }
}

void ThreadPool::workerThread(int id) {
    // Set thread affinity to core 1
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    std::cout << "Worker thread " << id << " started on core 1" << "\n";

    while (running) {
        void* taskData = nullptr;
        TaskNode* nodeToDelete = nullptr;

        // Lock the task queue for safe access
        {
            std::unique_lock<std::mutex> lock(listMutex);
            listCondition.wait(lock, [this] { 
                return !running || head != nullptr; 
            });
            
            if (!running && head == nullptr) {
                return;
            }
            
            // Retrieve the task from the front of the queue
            if (head != nullptr) {
                nodeToDelete = head;
                taskData = head->data;
                head = head->next;
                if (head == nullptr) {
                    tail = nullptr;
                }
            } else {
                continue;
            }
        }
        
        if (taskData != nullptr) {
            std::cout << "Worker thread " << id << " processing task..." << "\n";
            TaskData* data = static_cast<TaskData*>(taskData);
            data->callBackFunc();
            // Sleep to simulate work
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            delete data;
        }
        delete nodeToDelete;
    }
}
