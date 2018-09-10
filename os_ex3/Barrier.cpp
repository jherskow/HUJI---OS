#include "Barrier.h"
#include <cstdio>
#include <iostream>


Barrier::Barrier(int numThreads)
    : mutex(PTHREAD_MUTEX_INITIALIZER),
      cv(PTHREAD_COND_INITIALIZER),
      shuffMutex(PTHREAD_MUTEX_INITIALIZER),
      reduceMutex(PTHREAD_MUTEX_INITIALIZER),
      tvMutex(PTHREAD_MUTEX_INITIALIZER),
      count(0),
      numThreads(numThreads) {}

    void BarrierErrCheck(const std::string &message);

Barrier::~Barrier() {
    if (pthread_mutex_destroy(&mutex) != 0) { BarrierErrCheck("pthread_mutex_destroy");}

    if (pthread_cond_destroy(&cv) != 0) { BarrierErrCheck("pthread_cond_destroy");}

    if (pthread_mutex_destroy(&shuffMutex) != 0) { BarrierErrCheck("pthread_mutex_destroy");}

    if (pthread_mutex_destroy(&reduceMutex) != 0) { BarrierErrCheck("pthread_mutex_destroy");}

    if (pthread_mutex_destroy(&tvMutex) != 0) { BarrierErrCheck("pthread_mutex_destroy"); }
}

void Barrier::barrier() {
    if (pthread_mutex_lock(&mutex) != 0) { BarrierErrCheck("pthread_mutex_lock"); }

    if (++count < numThreads) {
        if (pthread_cond_wait(&cv, &mutex) != 0) { BarrierErrCheck("pthread_cond_wait"); }
    } else {
        count = 0;
        if (pthread_cond_broadcast(&cv) != 0) { BarrierErrCheck("pthread_cond_broadcast"); }
    }

    if (pthread_mutex_unlock(&mutex) != 0) { BarrierErrCheck("pthread_mutex_unlock"); }
}

void Barrier::mutex_lock(int lockID) {
    switch (lockID) {
        case THREADS_MUTEX:
            if (pthread_mutex_lock(&tvMutex) != 0){ BarrierErrCheck("pthread_mutex_lock"); }
            break;
        case SHUFFLE_MUTEX:
            if (pthread_mutex_lock(&shuffMutex) != 0) { BarrierErrCheck("pthread_mutex_lock"); }
            break;
        case REDUCE_MUTEX:
            if (pthread_mutex_lock(&reduceMutex) != 0) { BarrierErrCheck("pthread_mutex_lock"); }
            break;
        default:
            BarrierErrCheck("Requested to lock unknown mutex");

    }
}

void Barrier::mutex_unlock(int lockID) {
    switch (lockID) {
        case THREADS_MUTEX:
            if (pthread_mutex_unlock(&tvMutex) != 0){ BarrierErrCheck("pthread_mutex_unlock"); }
            break;
        case SHUFFLE_MUTEX:
            if (pthread_mutex_unlock(&shuffMutex) != 0) { BarrierErrCheck("pthread_mutex_unlock"); }
            break;
        case REDUCE_MUTEX:
            if (pthread_mutex_unlock(&reduceMutex) != 0) { BarrierErrCheck("pthread_mutex_unlock"); }
            break;
        default: BarrierErrCheck("Requested to unlock unknown mutex");

    }
}

////=================================  Error Function ==============================================

/**
 * Checks for failure of library functions, and handling them when they occur.
 */
void BarrierErrCheck(const std::string &message) {

    // set prefix
    std::string prefix = "[[Barrier]] error on ";

    // print error message with prefix
    std::cerr << prefix << message << "\n";

    // exit
    exit(1);

}