#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <fstream>
#include <string>

// Node structure for the linked list
struct TaskNode {
    void* data;
    TaskNode* next;
    
    TaskNode(void* d) : data(d), next(nullptr) {}
};

class ThreadPool {
public:
    ThreadPool();
    ~ThreadPool();

    /**
     * @brief Initializes the thread pool.
     * 
     * Reads the configuration file and initializes the thread pool with the specified
     * number of task pusher and worker threads.
     * 
     * @param configFile The file path to the configuration file.
     * @return True if the initialization succeeds, false otherwise.
     */
    bool initialize(const std::string& configFile);

    /**
     * @brief Push a task onto the thread pool's task queue.
     * 
     * Pushes a task onto the thread pool's task queue. The task will be executed
     * by one of the worker threads when it becomes available. The task is passed
     * as a raw pointer to the function.
     * 
     * @param data The task to be pushed onto the thread pool.
     */
    void pushTask(void* data);

    /**
     * @brief Gets the number of task pusher threads in the thread pool.
     * 
     * @return The number of task pusher threads in the thread pool.
     */
    int getNumTaskPushers() const { return numTaskPushers; }

    /**
     * @brief Gets the number of worker threads in the thread pool.
     * 
     * @return The number of worker threads in the thread pool.
     */
    int getNumWorkers() const { return numWorkers; }

private:
    /**
     * @brief Starts a task pusher thread that generates tasks at a fixed
     * rate.
     * 
     * The thread will push tasks onto the thread pool's task queue at a
     * rate determined by the sleep duration. The task type is determined
     * by the callback function passed to the thread pool's constructor.
     * 
     * @param id The ID number of the task pusher thread.
     */
    void taskPusherThread(int id);

    /**
     * @brief Function executed by each worker thread.
     * 
     * Worker threads process tasks from the task queue. Each thread is 
     * set to run on a specific CPU core for performance reasons.
     * 
     * @param id The ID number of the worker thread.
     */
    void workerThread(int id);

    int numTaskPushers;
    int numWorkers;
    
    std::vector<std::thread> taskPusherThreads;
    std::vector<std::thread> workerThreads;
    
    std::atomic<bool> running;
    
    TaskNode* head;
    TaskNode* tail;
    std::mutex listMutex;
    std::condition_variable listCondition;
};

#endif // THREAD_POOL_HPP