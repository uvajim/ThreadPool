#ifndef POOL_H_
#include <string>
#include <pthread.h>
#include <unordered_map>
#include <iostream>
#include <queue>
#include <semaphore.h>
#include <unordered_set>

class Task {
public:
    Task();
    virtual ~Task();
    bool is_completed;
    pthread_mutex_t is_completed_lock;
    pthread_cond_t task_completed_cond;

    virtual void Run() = 0;  // implemented by subclass
};

class ThreadPool {
public:
    ThreadPool(int num_threads);

    virtual ~ThreadPool();

    // Submit a task with a particular name.
    void SubmitTask(const std::string &name, Task *task);
 
    // Wait for a task by name, if it hasn't been waited for yet. Only returns after the task is completed.
    void WaitForTask(const std::string &name);

    void RunTask();


    // Stop all threads. All tasks must have been waited for before calling this.
    // You may assume that SubmitTask() is not caled after this is called.
    void Stop();
    
    //the thread pool
    pthread_t* pool;

    //the thread locks
    pthread_mutex_t queue_lock;
    pthread_mutex_t cipher_lock;
    pthread_mutex_t stop_lock;
    sem_t queue_semaphore;

    //condition variables
    pthread_cond_t wait_cond;
    pthread_cond_t queue_cond;

    int threads;


    //the thread pool data structures
    std::queue<Task*> task_queue;
    std::unordered_map<std::string, Task*> cipher;

    bool stop;


};
#endif
