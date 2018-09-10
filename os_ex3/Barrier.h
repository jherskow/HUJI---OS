#ifndef BARRIER_H
#define BARRIER_H
#include <pthread.h>

#define THREADS_MUTEX 1
#define SHUFFLE_MUTEX 2
#define REDUCE_MUTEX 3

// a multiple use barrier

class Barrier {
    public:
    Barrier(int numThreads);
    ~Barrier();
    void barrier();
    void mutex_lock(int lockID);
    void mutex_unlock(int lockID);

 private:
  pthread_mutex_t mutex;
  pthread_cond_t cv;
  pthread_mutex_t shuffMutex;
  pthread_mutex_t reduceMutex;
  pthread_mutex_t tvMutex;
  int count;
  int numThreads;
};

#endif //BARRIER_H
