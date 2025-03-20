# C++11 Thread Pool Implementation

This project implements a thread pool using C++11 threading capabilities. The thread pool manages a set of task pusher threads and worker threads to efficiently process tasks.

## Overview

The thread pool consists of two types of threads:
1. **Task Pusher Threads**: Responsible for pushing tasks into a shared task queue
2. **Worker Threads**: Responsible for processing tasks from the queue

Tasks are represented as a struct containing a callback function and two integer parameters. The thread pool uses a custom linked list to manage tasks and provides thread-safe access to the task queue.

## Project Structure

```
.
├── include/
│   ├── thread_pool.hpp    # Thread pool class definition
│   └── common_task.hpp    # Task data structure and common callback functions
├── src/
│   ├── thread_pool.cpp    # Thread pool implementation
│   ├── common_task.cpp    # Common task implementations
│   └── main.cpp           # Main program
├── build/                 # Build artifacts
├── obj/                   # Object files
├── threadpool_config.txt  # Configuration file
└── Makefile               # Build script
```

## Features

- Configurable number of task pusher and worker threads
- Thread affinity to specific CPU cores
- Thread-safe task management
- Custom linked list implementation for task queuing
- Support for different task types using function callbacks
- Dynamic memory management for task data

## Configuration

The number of task pusher and worker threads can be configured in the `threadpool_config.txt` file:

```
NUMS_OF_TASK_PUSHER 2
NUMS_OF_WORKER 4
```

## Task Types

The following task types are supported:
- Addition
- Subtraction
- Multiplication
- Division
- Fibonacci sequence generation

## Building and Running

### Prerequisites

- C++11 compatible compiler
- pthread library

### Build Commands

```bash
# Build the project
make

# Clean build artifacts
make clean

# Rebuild the project
make rebuild

# Run the program
make run
```

## Implementation Details

### Thread Pool

The thread pool is implemented in `thread_pool.hpp` and `thread_pool.cpp`. It provides the following functionalities:

- Thread creation and management
- Task queue management
- Thread synchronization using mutex and condition variables

### Task Management

Tasks are represented by the `TaskData` struct defined in `common_task.hpp`:

```cpp
struct TaskData {
    std::function<void(int, int)> callBackFunc;
    int value_1;
    int value_2;
};
```

Tasks are stored in a linked list and processed by worker threads in a FIFO manner.

### Memory Management

The thread pool uses raw pointers for task data. Memory is allocated when tasks are created and deallocated when tasks are processed. The thread pool ensures proper cleanup of any remaining tasks when it is destroyed.

## Example Usage

```cpp
#include "thread_pool.hpp"
#include "common_task.hpp"

int main() {
    ThreadPool pool;
    
    // Initialize the thread pool
    pool.initialize("threadpool_config.txt");
    
    // Create tasks
    TaskData* task1 = new TaskData{CommonTask::getAddTask(), 10, 20};
    TaskData* task2 = new TaskData{CommonTask::getSubtractTask(), 50, 30};
    
    // Push tasks to the pool
    pool.pushTask(static_cast<void*>(task1));
    pool.pushTask(static_cast<void*>(task2));
    
    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    return 0;
}
```

## License

This project is released under the MIT License. See the LICENSE file for details.