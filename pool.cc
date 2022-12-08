#include "pool.h"

void* init_thread(void* pool){
    ((ThreadPool*) pool)->RunTask();
    return NULL;
}

void ThreadPool::RunTask(){
    while(true){
        /*
            lock(queue)
            see if there's any awaiting task in the awaiting queue
                if there's none, you wait
                while() {
                    wait();
                    lock(stop)
                    if (stop) {
                        unlock all the locks you locked
                        unlock(queue)
                        unlock(stop)
                        return;
                    }
                    unlock(stop);
                }
            unlock(queue);
            pull out one task from the queue
            task->Run();
            lock(task->completed_lock);
            task->completed = true;
            unlock(task->completed_lock);
            signal(task->task_completed)

        */
        //we will wait if there are no items on the queue
        //sem_wait(&queue_semaphore);
        //if there are we will ensure that there is no race condition / two threads
        //grabbing the same task.
        
        pthread_mutex_lock(&queue_lock);
        while (task_queue.empty()){
            pthread_cond_wait(&queue_cond, &queue_lock);
            pthread_mutex_lock(&stop_lock);
            if (stop){
                pthread_mutex_unlock(&queue_lock);
                pthread_mutex_unlock(&stop_lock);
                pthread_cond_broadcast(&wait_cond);
                pthread_cond_broadcast(&queue_cond);
                return;
            }
            pthread_mutex_unlock(&stop_lock);
        }
        
        //we will take a task off the queue

        Task *curr_task = task_queue.front();
        task_queue.pop();
        pthread_mutex_unlock(&queue_lock);
        //run the task
        curr_task->Run();
        pthread_mutex_lock(&curr_task->is_completed_lock);
        curr_task->is_completed = true;
        pthread_cond_signal(&curr_task->task_completed_cond);
        pthread_mutex_unlock(&curr_task->is_completed_lock);
        //add the task to the completed tasks
        //deallocated the curr_task to free space
        //get the wait funtion to check if the task that is to be waited on is complete
        pthread_mutex_lock(&stop_lock);
         if (stop){
                pthread_mutex_unlock(&queue_lock);
                pthread_mutex_unlock(&stop_lock);
                pthread_cond_broadcast(&wait_cond);
                pthread_cond_broadcast(&queue_cond);
                return;
            }
        pthread_mutex_unlock(&stop_lock);

    }
    return;
}

Task::Task() {
    is_completed = false;
    pthread_mutex_init(&is_completed_lock, NULL);
    pthread_cond_init(&task_completed_cond, NULL);
}

Task::~Task() {
}

ThreadPool::ThreadPool(int num_threads) {
    pthread_mutex_init(&queue_lock, NULL);
    pthread_mutex_init(&cipher_lock, NULL);
    pthread_mutex_init(&stop_lock, NULL);
    pthread_cond_init(&wait_cond, NULL);
    pthread_cond_init(&queue_cond, NULL);
    stop = false;
    threads = num_threads;

    pool = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    for (int i = 0; i<num_threads; ++i) {
        //create pthread
        //make void* for class variables
        pthread_create(&pool[i], NULL, &init_thread, (void *)this);
    }    
}

ThreadPool::~ThreadPool(){
    for (int i = 0; i < threads; ++i){
        pthread_cond_broadcast(&queue_cond);
        pthread_join(pool[i], NULL);
    }
    free(pool);
    //task_queue.clear();
    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&cipher_lock);
    pthread_mutex_destroy(&stop_lock);
    pthread_cond_destroy(&wait_cond);
    pthread_cond_destroy(&queue_cond);

}

void ThreadPool::SubmitTask(const std::string &name, Task* task) {
    //task is now being added
    pthread_mutex_lock(&queue_lock);
    //push the task onto the queue
    task_queue.push(task);
    pthread_mutex_unlock(&queue_lock);
    pthread_mutex_lock(&cipher_lock);
    //since the param in WaitForTask is a name, we will use a cipher to determine the task.
    //ASSUMPTION: No task can concurrently share the same name
    cipher[name] = task;
    //unlock since we no longer need this stuff
    pthread_mutex_unlock(&cipher_lock);
    pthread_cond_broadcast(&queue_cond);
}

void ThreadPool::WaitForTask(const std::string &name) {
    /*
        get the Task using the name from the cipher
        there should be a variable stored in each Task that tracks whether the task is copmleted or not
        lock(task->copmleted_lock);
        while (!task->copmleted) {
            wait for "completed" to be true
            pthread_cond_wait(task->is_copmleted, task->completed_lock);
        }
        lock(task->copmleted_lock);
        remove the task from the cipher
        delete task
    */
   pthread_mutex_lock(&cipher_lock);
   Task * in_prog_task = cipher[name];
   pthread_mutex_unlock(&cipher_lock);
    pthread_mutex_lock(&in_prog_task->is_completed_lock);
    while(!in_prog_task->is_completed){
        pthread_cond_wait(&in_prog_task->task_completed_cond, &in_prog_task->is_completed_lock);
    }
    //reacquire the lock
    //pthread_mutex_lock(&in_prog_task->is_completed_lock);
    //since wait for task is only called once per task, we can remove 
    //the task from the set once we know that this has been finished. 
    pthread_mutex_lock(&cipher_lock);
    cipher.erase(name);
    //since wait for task is only called once per task, we can remove 
    //the task from the map once we know that this has been finished as we
    //no longer have to keep track of the task name. 
    pthread_mutex_unlock(&cipher_lock);
    delete in_prog_task;
    return;
}

void ThreadPool::Stop() {
    pthread_mutex_lock(&stop_lock);
    stop = true;
    pthread_mutex_unlock(&stop_lock);
    pthread_cond_broadcast(&wait_cond);
    pthread_cond_broadcast(&queue_cond);
    return;
}
