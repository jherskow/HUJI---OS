#include <pthread.h>
#include <cstdio>
#include <atomic>
#include "semaphore.h"

#define MT_LEVEL 5

struct ThreadContext {
    std::atomic<int>* atomic_counter;
    int* bad_counter;
    sem_t * sem;
    int tid;
};


void* foo(void* arg)
{
    ThreadContext* tc = (ThreadContext*) arg;

    printf("before sem_wait current tid:%d \n", tc->tid);
    //if (tc->tid != 0) { sem_wait(tc->sem); }
    sem_wait(tc->sem);

    printf("woken up current tid:%d \n", tc->tid);
    for (int i = 0; i < 1000; ++i) {
	if (i == 0) { printf("loop current tid:%d \n", tc->tid); }
        // old_value isn't used in this example, but will be necessary
        // in the exercise
        int old_value = (*(tc->atomic_counter))++;
        (*(tc->bad_counter))++;
    }
    //std::atomic<int> ata = *(tc->atomic_counter);
    printf("after loop current tid:%d atomic:%d bad:%d \n", tc->tid, tc->atomic_counter->load(), *(tc->bad_counter));
//    if (tc->tid == 0) { sem_post(tc->sem); }
    sem_post(tc->sem);
    return 0;
}


int main(int argc, char** argv)
{
    pthread_t threads[MT_LEVEL];
    ThreadContext contexts[MT_LEVEL];
    std::atomic<int> atomic_counter(0);
    int bad_counter = 0;
    sem_t sem1;

    for (int i = 0; i < MT_LEVEL; ++i) {
        contexts[i] = {&atomic_counter, &bad_counter, &sem1, i};
    }
    printf("before init sem\n");
    
    sem_init(&sem1, 0, 0);
//    sem_init(&sem1, 0, 1);
    for (int i = 0; i < MT_LEVEL; ++i) {
        pthread_create(threads + i, NULL, foo, contexts + i);
    }
    
    printf("just having fun\n");
    for (int i = 0; i < MT_LEVEL; ++i) {
        pthread_join(threads[i], NULL);
    }
    printf("atomic counter: %d\n", atomic_counter.load());
    printf("bad counter: %d\n", bad_counter);
    
    return 0;
}

