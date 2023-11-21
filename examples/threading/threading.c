#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
// #define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    DEBUG_LOG("New thread started");

    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    thread_func_args->thread_complete_success = true;

    int rc;

    usleep(thread_func_args->wait_to_obtain_ms*1000);

    DEBUG_LOG("Trying to lock mutex");
    rc = pthread_mutex_lock(thread_func_args->mutex);
    if(rc != 0)
    {
        ERROR_LOG("Locking mutex failed");
        thread_func_args->thread_complete_success = false;
    }
    else
    {
        DEBUG_LOG("Locking mutex passed");
    }
    usleep(thread_func_args->wait_to_release_ms*1000);
    DEBUG_LOG("Unlocking mutex");
    rc = pthread_mutex_unlock(thread_func_args->mutex);
    if(rc != 0)
    {
        ERROR_LOG("Unlocking mutex failed");
        thread_func_args->thread_complete_success = false;
    }
    else
    {
        DEBUG_LOG("Unlocking mutex passed");
    }
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    int rc;
    bool retVal = true;
    
    struct thread_data *thread_param = (struct thread_data *) malloc(sizeof(struct thread_data));
    
    thread_param->mutex = mutex;
    thread_param->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_param->wait_to_release_ms = wait_to_release_ms;

    rc = pthread_create(thread, NULL, &threadfunc, (void *)thread_param);
    if(rc != 0)
    {
        ERROR_LOG("Creating thread failed");
        retVal = false;
    }
    else
    {
        DEBUG_LOG("Creating thread successed");
    }
    return retVal;
}

