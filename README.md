# ThreadPool.hpp

A Super Light Header Only No Exception C++ `ThreadPool` Implementation

# Basic Usage Demo
Cross-platform, but at minimum, a `C++20` compiler is required.
```cpp
#include <cmath>
#include <iostream>
#include "ThreadPool.hpp"

void thread_worker(int id, size_t j) noexcept {
    size_t cum = 0;
    for(size_t i = 0; i < j; ++i){
        cum += i;
    }
    std::cout << "Thread " << id << " outputs "<< cum << std::endl;
    return;
}

void long_duration_worker(int id, size_t j) noexcept {
    double aob = 0;
    for(size_t i = 0; i < j; ++i) {
        aob += sin(cos(static_cast<double>(j)));
    }
    std::cout << "Thread " << id << " outputs "<< aob << std::endl;
    return;
}

int main(){

    ThreadPool tp(4); 
    // Create a threadpool with 4 threads

    for(int i = 0; i < 10; i += 2){
        tp.Invoke(thread_worker, i, 114514+i); // Invoke some tasks
        tp.Invoke(long_duration_worker, i+1, 11451414+i+1);
    }

    tp.WaitTillNoPending();  
    // Wait until all job are running

    std::cout << "There are currently " << tp.StartedNum() - tp.FinishedNum() << " jobs out of " << tp.StartedNum() << " jobs are running ..." << std::endl;
    // Inspect jobs

    for(int i = 10; i < 20; ++i) {
        tp.Invoke(thread_worker, i, 114514+i); // Continue to invoke some tasks
    }

    std::cout << "There are currently " << tp.StartedNum() - tp.FinishedNum() << " jobs out of " << tp.StartedNum() << " jobs are running ..." << std::endl;
    // Inspect jobs
    
    tp.WaitTillAll(); // Wait until everything is done
    // Always use this to join(). or using .Detach()

    return 0;
}
```

# To Do
Support checking whether a certain task is completed or not.

Support callback.
