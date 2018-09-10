
#include <atomic>
#include <algorithm>
#include <iostream>
#include <semaphore.h>

#include "MapReduceClient.h"
#include "MapReduceFramework.h"
#include "Barrier.h"


//// ============================   defines and const ==============================================


//// ===========================   typedefs & structs ==============================================

/**
 * This struct is uniquely created for each thread.
 * It holds pointers to entire shared-data between threads.
 */
struct ThreadContext {
  // unique fields
  unsigned long threadID;

  // shared data among threads
  const MapReduceClient *client;
  const InputVec *inputVec;
  OutputVec *outputVec;
  bool *stillShuffling;
  std::atomic<unsigned int> *atomic_counter;
  sem_t *semaphore_arg;
  Barrier *barrier;
  std::vector<IntermediateVec> *threadsVectors;
  std::vector<IntermediateVec> *shuffleVector;
};


//// ============================   forward declarations for helper funcs ==========================

// thread entry point.
void * workerThreadFlow(void * arg);

// Map-Reduce algorithm phases.
void mapAndSort(ThreadContext * tc);
void shuffle(ThreadContext *tc, unsigned long numOfThreads);
void reduce(ThreadContext *tc);

// Utilities for managing the framework library.
void exitFramework(ThreadContext *tc);
void errCheck(int &returnVal, const std::string &message);

// K2 helpers
bool areEqualK2(K2 &a, K2 &b);
bool compareKeys(const IntermediatePair &a, const IntermediatePair &b);


//// ============================ framework functions ==============================================

/**
 *
 * @param key a pointer to K2.key object
 * @param value a pointer to K2.value object
 * @param context thread context
 */
void emit2(K2 *key, V2 *value, void *context) {
    auto tc = (ThreadContext *) context;
    IntermediatePair k2_pair = std::make_pair(key, value);
    tc->barrier->mutex_lock(THREADS_MUTEX);
    tc->threadsVectors->at(tc->threadID).push_back(k2_pair);
    tc->barrier->mutex_unlock(THREADS_MUTEX);
}

/**
 *
 * @param key a pointer to K3.key object
 * @param value a pointer to K3.value object
 * @param context thread context
 */
void emit3(K3 *key, V3 *value, void *context) {
    auto *tc = (ThreadContext *) context;
    OutputPair k3_pair = std::make_pair(key, value);
    tc->outputVec->push_back(k3_pair);
}


/**
 * Runs the framework according to specified input.
 * @param client - Ref to a client.
 * @param inputVec - Ref to input.
 * @param outputVec - Ref to output.
 * @param multiThreadLevel - # of threads the framework should use.
 */
void runMapReduceFramework(const MapReduceClient &client, const InputVec &inputVec,
                           OutputVec &outputVec, int multiThreadLevel) {

    // Validation Assumption multiThreadLevel argument is greater or equal to 1
    auto numOfThreads = (unsigned long) multiThreadLevel;

    //// -------   Variables -------

    pthread_t threads[multiThreadLevel];
    ThreadContext threadContexts[multiThreadLevel];
    Barrier barrier(multiThreadLevel);
    bool everydayImShuffling = true;
    std::atomic<unsigned int> atomic_counter(0);
    std::vector<IntermediateVec> threadsVectors(numOfThreads, IntermediateVec(0));
    std::vector<IntermediateVec> shuffleVector(0);

    //// -------   Initialisation -------

    // init semaphore so other threads would wait to it.
    sem_t sem;

    int sysRetVal = sem_init(&sem, 0, 0);
    errCheck(sysRetVal, "sem_init");

    // init thread contexts
    for (unsigned long i = 0; i < numOfThreads; ++i) {
        threadContexts[i] = {i, &client, &inputVec, &outputVec, &everydayImShuffling,
                             &atomic_counter, &sem, &barrier, &threadsVectors, &shuffleVector};
    }

    //// -------   Creating workers threads -------

    // init threads to run workerThreadFlow function.
    for (int i = 1; i < multiThreadLevel; ++i) {
        sysRetVal = pthread_create(threads + i, nullptr, workerThreadFlow, threadContexts + i);
        errCheck(sysRetVal, "pthread_create");
    }

    // main thread should map and sort as well.
    mapAndSort(threadContexts);

    //// -------   Shuffle & Reduce -------

    shuffle(&threadContexts[0], numOfThreads);

    // main thread should reduce one shuffle is done
    for (int i = 0; i < multiThreadLevel; i++) {
        sysRetVal = sem_post(threadContexts->semaphore_arg);
        errCheck(sysRetVal, "sem_post");
    }

    // main thread should reduce one shuffle is done
    reduce(threadContexts);


    // main thread will wait for all other threads to terminate
    for (int i = 1; i < multiThreadLevel; ++i) {
        sysRetVal = pthread_join(threads[i], nullptr);
        errCheck(sysRetVal, "pthread_join");
    }

    //// -------   Cleanup & Finish -------
    exitFramework(threadContexts);

}

////===============================  Helper Functions ==============================================

/**
 * Handles the entire worker-thread flow. (not main thread)
 * @param arg - a pointer to a ThreadContext struct.
 * @return null.
 */
void * workerThreadFlow(void * arg) {
    auto tc = (ThreadContext *) arg;
    mapAndSort(tc);
    reduce(tc);
    return nullptr;
}

/**
 * Handles the map and sort phases for any thread.
 * @param tc a pointer to a ThreadContext struct.
 */
void mapAndSort(ThreadContext * tc) {

    // Map phase
    bool shouldContinueMapping = true;
    while (shouldContinueMapping) {

        // check atomic for new items to be mapped (k1v1)
        unsigned int old_atom = (*(tc->atomic_counter))++;
        if (old_atom < (tc->inputVec->size())) {

            // calls client map func with pair at old_atom.
            tc->client->map(tc->inputVec->at(old_atom).first,
                            tc->inputVec->at(old_atom).second, tc);
        } else {
            //done parsing the input vector.
            shouldContinueMapping = false;
        }
    }

    // Sort phase
    if (!tc->threadsVectors->at(tc->threadID).empty()) {
        std::sort(tc->threadsVectors->at(tc->threadID).begin(),
                  tc->threadsVectors->at(tc->threadID).end(), compareKeys);
    }

    tc->barrier->barrier(); // wait at the barrier
}


/**
 * Performs a shuffle. Called only by main thread.
 * @param tc a pointer to a ThreadContext struct.
 * @param multiThreadLevel number of threads to make.
 */
void shuffle(ThreadContext *tc, unsigned long numOfThreads) {

    // init atomic to track output vec part of the reduce method
    (*tc->atomic_counter) = 0;

    // this while will keep loop until we have shuffled all pairs.
    while (true) {      //everyday im shufflin'

        // flags
        unsigned long maxKeyThreadId = 0;
        K2 *key = nullptr;

        // find the first not empty thread Intermediate Vector.
        for (unsigned long i = 0; i < numOfThreads; i++) {

            // take the back of this vector.
            if (!tc->threadsVectors->at(i).empty()) {
                maxKeyThreadId = i;
                key = tc->threadsVectors->at(i).back().first;
                break;
            }
        }

        // if we didn't find non-empty thread Vector, we are done.
        if (key == nullptr) {
            break;
        }

        // Finding the max key
        for (unsigned long i = maxKeyThreadId; i < numOfThreads; i++) {

            // for each non-empty vector
            if (tc->threadsVectors->at(i).empty()) { continue; }

            // replace max if this key is greater.
            if (!tc->threadsVectors->at(i).empty() &&
                (*key) < *(tc->threadsVectors->at(i).back().first)) {

                maxKeyThreadId = i;
                key = tc->threadsVectors->at(i).back().first;
            }
        }

        //// create a vector from all the K2 s.t. K2=max
        IntermediateVec currentKeyIndVec(0);

        // for all vectors
        for (unsigned long i = maxKeyThreadId; i < numOfThreads; i++) {

            // while this thread's vector is not empty
            while (!tc->threadsVectors->at(i).empty() &&
                areEqualK2(*(tc->threadsVectors->at(i).back().first), *key)) {

                // take back iff the back is equal to max
                tc->barrier->mutex_lock(THREADS_MUTEX);
                currentKeyIndVec.push_back((tc->threadsVectors->at(i).back()));
                tc->threadsVectors->at(i).pop_back();
                tc->barrier->mutex_unlock(THREADS_MUTEX);
            }
        }

        // blocking the mutex
        tc->barrier->mutex_lock(SHUFFLE_MUTEX);

        //// send vector to a thread, and wake using semaphore

        // feeding shared vector and increasing semaphore.
        tc->shuffleVector->emplace_back(currentKeyIndVec);
        (*(tc->atomic_counter))++;

        int sysRetVal = sem_post(tc->semaphore_arg);
        errCheck(sysRetVal, "sem_post");
        tc->barrier->mutex_unlock(SHUFFLE_MUTEX);
    }

    // notify threads shuffling has ended - threads should now move to reduce if semaphore is zero.
    *tc->stillShuffling = false;
}


/**
 * Performs reduce within a thread context.
 */
void reduce(ThreadContext *tc) {
    while (true) {  // this while will keep loop until we have reduced all K2,V2 pairs.

        // call wait
        int sysRetVal = sem_wait(tc->semaphore_arg);
        errCheck(sysRetVal, "sem_wait");

        int atom = (*(tc->atomic_counter))--;

        if (atom <= 0) {
            break;
        }

        //taking new vector from shuffle.
        tc->barrier->mutex_lock(SHUFFLE_MUTEX);
        IntermediateVec pairs = (tc->shuffleVector->back());

        tc->shuffleVector->pop_back();
        tc->barrier->mutex_unlock(SHUFFLE_MUTEX);

        // reducing the Intermediate vector.
        tc->barrier->mutex_lock(REDUCE_MUTEX);
        tc->client->reduce(&pairs, tc);
        tc->barrier->mutex_unlock(REDUCE_MUTEX);

    }
}

/**
 * Simple by-K2 lesser-than comparator for intermediatePair.
 * @return true iff lhs's K2 is smaller.
 */
bool compareKeys(const IntermediatePair &a, const IntermediatePair &b) {
    return (*a.first) < (*b.first);
}

/**
 * Simple Equality checker for K2 keys.
 */
bool areEqualK2(K2 &a, K2 &b) {
    // neither a<b nor b<a means a==b
    return !((a < b) || (b < a));
}

/**
 * Cleans up framework before exiting
 */
void exitFramework(ThreadContext *tc) {

    int retVal = sem_destroy(tc->semaphore_arg);
    errCheck(retVal, "sem_destroy");
}

////=================================  Error Function ==============================================

/**
 * Checks for failure of library functions, and handling them when they occur.
 */
void errCheck(int &returnVal, const std::string &message) {

    // if no failure, return
    if (returnVal == 0) return;

    // set prefix
    std::string prefix = "[[Framework]] error on ";

    // print error message with prefix
    std::cerr << prefix << message << "\n";

    // exit
    exit(1);
}

