
#include "uthreads.h"

#include <iostream>
#include <deque>
#include <functional>
#include <vector>
#include <queue>
#include "thread.h"


//// ============================   defines and const ==============================================

#define SYS_ERR 406 // Not acceptable
#define THRD_ERR 418    // I'm a teapot
#define SEC_IN_MICROSEC 1000000

//// ============================   fields =========================================================

Thread *threadList[MAX_THREAD_NUM]{nullptr};
std::deque<Thread *> readyThreads;
Thread *running_thread = nullptr;
std::priority_queue<int, std::vector<int>, std::greater<int> > tidMinHeap;

struct itimerval timer; // the interval timer
struct sigaction sa;    // the sigaction defined for SIGVTALRM.
sigset_t blockedSignalSet;  // set of signals to block on critical code-blocks.
int totalQuantumsRunning;


//// ============================   forward declarations for helper funcs ==========================

//// scheduling
void timesUp(int sig);

void goToNextThread();

void selfBlockAdjustment();

void removeFromReady(const Thread *toRemove);

//// dependencies
void reviveDependants(int tid);

void clearSyncTo(int tid);

//// naming
void initialiseMinHeap();

int isLegalTid(int tid);

//// memory
void freeAllMemory();

void terminateThread(Thread *thread);

//// errors
void print_error(int type, const std::string &message = "unknown error");

//// signal blocking
void blockSignals();

void unblockSignals();

void unblockSignalsAndIgnorePending();

//// initialisation
void initTimerHandler();

void initQuant(int quantum_usecs);

int spawnMainThread();

//// ============================   library functions ==============================================

/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs) {

  // initialise a min heap for all legal tid's (including 0 )
  initialiseMinHeap();

  // set constants ( quantum, etc) (error handling)
  if (quantum_usecs <= 0) {
    print_error(THRD_ERR, "Quantum must be strictly positive.");
    return -1;
  }

  // configure quantum and timer.
  initQuant(quantum_usecs);

  // spawn main thread
  int status = spawnMainThread();
  if (status != 0) { return status; }

  // install timesUp() as handler for timer signals
  initTimerHandler();

  // set first running thread
  running_thread = readyThreads.front();
  readyThreads.pop_front();

  // set the timer.
  if (setitimer(ITIMER_VIRTUAL, &timer, nullptr) < 0) {
    print_error(SYS_ERR, "Call to setitimer failed.");
  }

  // all done!
  return 0;
}

/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)(void)) {

  blockSignals();
  // get new tid from min heap - if none - we are at limit.
  int newId = tidMinHeap.top();
  if (newId == MAX_THREAD_NUM) {
    print_error(THRD_ERR, "Attempted to spawn thread beyond max thread limit.");
    unblockSignals();
    return -1;
  }

  // create thread (and allocate stack (in thread or here? )) (check success)
  Thread *threadToSpawn;
  try {
    threadToSpawn = new Thread(newId, f);
  } catch (std::exception &e) {
    print_error(SYS_ERR, "Call to new failed for Thread.");
  }
  threadList[newId] = threadToSpawn;
  tidMinHeap.pop();
  readyThreads.push_back(threadToSpawn);
  unblockSignals();
  return newId;
}

/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid) {

  // block thread's signals for duration of this function.
  blockSignals();

  // error handling
  if (!isLegalTid(tid)) {
    print_error(THRD_ERR, "Terminate called with illegal thread id.");

    // unblock and error
    unblockSignals();
    return -1;
  }

  // if terminating thread is the main thread (tid = 0)
  // call d-tor for all threads + exit(0)
  if (tid == 0) {
    freeAllMemory();
    exit(0);
  }

  // move waiting threads to ready if possible.
  reviveDependants(tid);

  // remove terminating thread from mater's waitlist, if it is on one.
  clearSyncTo(tid);

  // if terminated thread is running thread - scheduling decision (jump)
  if (tid == uthread_get_tid()) {

    // delete running thread
    terminateThread(running_thread);

    // unblock signals and ignored pending because we are making scheduling decision anyway.
    unblockSignalsAndIgnorePending();

    // go to next ready thread
    // (there must be one, because the running thread at this point cannot be main)
    goToNextThread();

  } else {

    // case: other (non running) thread is terminated  - data change only, and return
    // get rid of terminated thread:
    Thread *terminate = threadList[tid];

    // if in ready, remove it
    removeFromReady(terminate);

    // free it from threadlists
    terminateThread(terminate);

    // unblock and finish
    unblockSignalsAndIgnorePending();
  }
  return 0;
}

/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid) {

  // block thread's signals for duration of this function.
  blockSignals();


  // error handling
  if (!isLegalTid(tid)) {

    print_error(THRD_ERR, "Block called with illegal thread id.");

    // unblock and error
    unblockSignals();
    return -1;
  }

  if (tid == 0) {

    print_error(THRD_ERR, "Attempted to block main thread.");

    // unblock and error
    unblockSignals();
    return -1;
  }

  if (threadList[tid]->getIsWaitingToResume()) {

    // unblock and finish
    unblockSignals();
    return 0;
  }

  // if the running thread is blocking itself, call the selfBlock function
  // this will also take care of jumping to the next thread
  if (tid == uthread_get_tid()) {

    //change thread status to blocked
    running_thread->blockThread();
    selfBlockAdjustment();
    // else if ready:   just move to blocked
  } else {

    Thread *threadToBlock = threadList[tid];
    threadToBlock->blockThread();
    for (unsigned int i = 0; i < readyThreads.size(); ++i) {
      if (readyThreads[i] == threadToBlock) {

        readyThreads.erase(readyThreads.begin() + i);
      }
    }

    // unblock and finish
    unblockSignals();

  }
  return 0;
}

/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {

  // block thread's signals for duration of this function.
  blockSignals();


  // error handling
  if (!isLegalTid(tid)) {

    print_error(THRD_ERR, "Resume called with illegal thread id.");

    // unblock and error
    unblockSignals();
    return -1;
  }
  Thread *threadToResume = threadList[tid];

  if (threadToResume->getIsWaitingToResume()) {

    threadToResume->unblockThread();

    if (threadToResume->getSyncedTo() == NOT_SYNCED) {

      threadToResume->setStatus(ready);

      readyThreads.push_back(threadToResume);
    }
  }

  // unblock and finish
  unblockSignals();
  return 0;
}

/*
 * Description: This function blocks the RUNNING thread until thread with
 * ID tid will terminate. It is considered an error if no thread with ID tid
 * exists or if the main thread (tid==0) calls this function. Immediately after the
 * RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_sync(int tid) {

  // block thread's signals for duration of this function.
  blockSignals();

  // error handling
  if (!isLegalTid(tid)) {

    print_error(THRD_ERR, "Sync called with illegal thread id.");
    // unblock and error
    unblockSignals();
    return -1;
  }

  if (tid == uthread_get_tid()) {

    print_error(THRD_ERR, "Thread attempted Sync to itself.");
    // unblock and error
    unblockSignals();
    return -1;
  }

  if (uthread_get_tid() == 0) {

    print_error(THRD_ERR, "Main thread attempted to call Sync.");
    // unblock and error
    unblockSignals();
    return -1;
  }

  // change RUNNING's sycedTo field to match the given tid
  running_thread->setSyncedTo(tid);
  // change RUNNING's status to blocked
  running_thread->setStatus(blocked);

  // append &(RUNNING) to (tid)'s  dependents queue(which is a field of *(tid))
  threadList[tid]->addToDependants(running_thread);

  // make the self-block scheduling adjustment
  selfBlockAdjustment();

  return 0;
}

/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid() {
  return running_thread->getTid();
}

/*
 * Description: This function returns the total number of quantums since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums() {
  // return value from quantum counter.
  return totalQuantumsRunning;

}

/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantums of the thread with ID tid.
 * 			     On failure, return -1.
*/
int uthread_get_quantums(int tid) {

  //error checking
  if (!isLegalTid(tid)) {
    print_error(THRD_ERR, "Get quantums called with illegal thread id.");
    return -1;
  }

  // return value from *(tid)'s quantum counter  (which is a field of *(tid))
  return threadList[tid]->getQuantumRunTime();

}


////===============================  Helper Functions ==============================================

//// ------------------------  scheduling ----------------------------------------------------------


/** called if a quantum has ended (no blocking necessary)
 * (however, there might only be a main thread)
 * */
void timesUp(int sig) {

  // if there are other threads waiting
  if (readyThreads.front() != nullptr) {
    // save the program before jump (with 1 to save signal mask)

    int retVal = sigsetjmp(running_thread->tEnv, 1);

    // if we have arrived here after a jump, - return from the function
    // and continue executing the uThread's code.
    if (retVal == POST_JUMP) { return; }

    // change this thread's status to ready
    running_thread->setStatus(ready);

    //append to ready
    readyThreads.push_back(running_thread);

  }
  // go to next thread
  goToNextThread();
}

/** switches to next thread
 *
 * this function should be called AFTER running has been placed in its desired queue
 * (READY, BLOCKED, or even terminated)
 * */
void goToNextThread() {

  // increment global timer
  totalQuantumsRunning++;

  // check and see if there are ready threads
  if (readyThreads.front() == nullptr) {
    // increment main thread's timer
    running_thread->incrementQuantumRunTime();
    return;
  }

  // pop top ready to running
  running_thread = readyThreads.front();
  readyThreads.pop_front();

  // set the thread status to running
  running_thread->setStatus(running);

  // increment running thread's timer
  running_thread->incrementQuantumRunTime();

  // resetting timer
  if (setitimer(ITIMER_VIRTUAL, &timer, nullptr) < 0) {
    print_error(SYS_ERR, "setitimer system error.");
  }
  siglongjmp(running_thread->tEnv, POST_JUMP);
}

/**
 * Run only in case of self block (not self terminate)
 * @return
 */
void selfBlockAdjustment() {

  // unblock signals and ignored pending because we are making scheduling decision anyway.
  unblockSignalsAndIgnorePending();

  // save the program before jump
  int retVal = sigsetjmp(running_thread->tEnv, 1);

  if (retVal == POST_JUMP) {

    // this thread will pick up from here once it returns to running
    return;
  }
  // get the next thread from running.
  goToNextThread();
}

void removeFromReady(const Thread *toRemove) {
  for (unsigned int i = 0; i < readyThreads.size(); ++i) {
    if (readyThreads[i] == toRemove) {
      readyThreads.erase(readyThreads.begin() + i);
    }
  }
}

//// ------------------------  dependencies --------------------------------------------------------

/**
 *  This method checks to see if any threads are waiting for thread tid to terminate.
 *  If there are, the method will change them to "ready" status if posible.
 *  If a dependant is also waiting for a RESUME call, this method only clears
 *  the 'SynchedTo' flag.
 *
 * @param tid - tid of thread whose dependants we wish to restore.
 */
void reviveDependants(int tid) {
  std::deque<Thread *> *dependants = threadList[tid]->getThreadDependants();

  // if this thread has dependant threads waiting for termination:
  while (!dependants->empty()) {

    // set waiting thread's status to not syncd
    dependants->front()->setSyncedTo(NOT_SYNCED);

    // if this thread is not waiting for a RESUME call
    if (!dependants->front()->getIsWaitingToResume()) {


      // change the thread's status
      dependants->front()->setStatus(ready);
      // append the thread to ready
      readyThreads.push_back((dependants->front()));
    }

    // pop and continue to the next dependant
    dependants->pop_front();
  }
}

/**
 * This method checks to see if a terminating thread is listed in another thread's waitlist.
 * If so, it will remove it from that list.
 * This method should only be called during termination of thread tid.
 *
 * @param tid - tid of thread to clear
 */
void clearSyncTo(int tid) {

  // find the master thread
  int syncTo = threadList[tid]->getSyncedTo();

  // if thread has a master
  if (syncTo != NOT_SYNCED) {

    // remove tid from the master's waitlist.
    threadList[syncTo]->clearSyncDep(threadList[tid]);
  }
}
//// ------------------------  naming --------------------------------------------------------------

void initialiseMinHeap() {

  for (int i = 0; i <= MAX_THREAD_NUM; ++i) // zero-indexed to add main as well.
  {
    tidMinHeap.push(i);
  }
}

int isLegalTid(int tid) {

  // checks if this is an existing thread
  return (tid < MAX_THREAD_NUM && threadList[tid] != nullptr);
}

//// ------------------------  memory --------------------------------------------------------------

void freeAllMemory() {

  for (auto &threadObj : threadList) // zero-indexed to delete main as well.
  {
    delete threadObj;
  }
}

/**
 * Terminates a thread and reclaims its tid.
 * @param thread
 */
void terminateThread(Thread *thread) {
  int tid = thread->getTid();
  delete thread;
  threadList[tid] = nullptr;
  tidMinHeap.push(tid);
}

//// ------------------------  errors --------------------------------------------------------------

void print_error(int type, const std::string &message) {

  // set prefix
  std::string title = (type == SYS_ERR) ? "system error: " : "thread library error: ";

  // print error message with prefix
  std::cerr << title << message << "\n";

  // if error is a system error - quit with status 1
  if (type == SYS_ERR) {

    //release memory
    freeAllMemory();

    // exit
    exit(1);
  }
}

//// ------------------------  signal blocking------------------------------------------------------

/**
 * This method will block the signal of SIGVTALRM until unblock is called explicitly.
 */
void blockSignals() {

  if (sigprocmask(SIG_BLOCK, &blockedSignalSet, NULL) < 0) {
    print_error(SYS_ERR, "System failed to block signals.");
  }
}

/**
 * This method will unblock the signal of SIGVTALRM if it already been blocked explicitly.
 */
void unblockSignals() {

  if (sigprocmask(SIG_UNBLOCK, &blockedSignalSet, NULL) < 0) {
    print_error(SYS_ERR, "System failed to unblock signals.");
  }
}

/**
 * This method will unblock the signal of SIGVTALRM if it already been blocked explicitly.
 */
void unblockSignalsAndIgnorePending() {

  if (sigemptyset(&blockedSignalSet) < 0) {
    print_error(SYS_ERR, "Couldn't empty blockSignalSet during unblocking & ignoring.");
  }

  if (sigaddset(&blockedSignalSet, SIGVTALRM) < 0) {
    print_error(SYS_ERR, "Couldn't add signal to blockSignalSet during unblocking & ignoring.");
  }

  if (sigprocmask(SIG_UNBLOCK, &blockedSignalSet, NULL) < 0) {
    print_error(SYS_ERR, "System failed to unblock signals.");
  }
}

//// ------------------------  initialisation  -----------------------------------------------------

void initTimerHandler() {

  sa.sa_handler = &timesUp; // Install timesUp() as the signal handler for SIGVTALRM.
  if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
    print_error(SYS_ERR, "Failed to install sigaction handler.");
  }
  if (sigemptyset(&blockedSignalSet) < 0) {
    print_error(SYS_ERR, "Couldn't empty blockSignalSet during init");
  }
  if (sigaddset(&blockedSignalSet, SIGVTALRM) < 0) {
    print_error(SYS_ERR, "Couldn't add signal to blockSignalSet during init");
  }
}

void initQuant(int quantum_usecs) {

  // Configure the timer to expire every quantum.*/
  timer.it_value.tv_sec = timer.it_interval.tv_sec = quantum_usecs / SEC_IN_MICROSEC;
  timer.it_value.tv_usec = timer.it_interval.tv_usec = quantum_usecs % SEC_IN_MICROSEC;
  // first time interval, uSec part
}

int spawnMainThread() {

  // spawn main thread (tid=0)
  int ret_val = uthread_spawn(nullptr);

  // if spawning the main thread failed
  if (ret_val == -1) {
    print_error(THRD_ERR, "UThreads library was failed spawning main thread");
    return -1;
  }

  // start a quantum for the main thread and for the global quantum
  threadList[ret_val]->incrementQuantumRunTime();
  totalQuantumsRunning = 1;

  return 0;
}