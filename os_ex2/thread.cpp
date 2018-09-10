
#include "thread.h"


////===========================================   processor stuff ==================================
#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr) {
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
      "rol    $0x11,%0\n"
  : "=g" (ret)
  : "0" (addr));
  return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
        "rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif
//// ========================================= END  processor stuff ================================


/**
 * Constructor of Thread object.
 * @param tid the unique id of the new thread
 * @param f pointer to function
 */
Thread::Thread(int tid, void (*f)(void)) {

  this->tid = tid;
  tStatus = (tid == 0) ? running : ready;

  quantumRunningCounter = 0;
  isWaitingToResume = false;
  syncedTo = NOT_SYNCED;
  auto sp = (address_t) this->tStack + STACK_SIZE - sizeof(address_t);
  auto pc = (address_t) f;
  sigsetjmp(tEnv, 1);
  (tEnv->__jmpbuf)[JB_SP] = translate_address(sp);
  (tEnv->__jmpbuf)[JB_PC] = translate_address(pc);
  sigemptyset(&(tEnv->__saved_mask));
}

/**
 * D-tor of Thread object.
 */
Thread::~Thread() = default;

/**
 * @return the current threadId.
 */
int Thread::getTid() {
  return this->tid;
}

/**
 * @return the current thread status.
 */
ThreadStatus Thread::getStatus() {
  return this->tStatus;
}

/**
 * Sets the thread status
 * @param status - status to set
 */
void Thread::setStatus(ThreadStatus status) {
  tStatus = status;
}

/**
 * @return true iff thread is waiting to resume (thread was explicitly blocked).
 */
bool Thread::getIsWaitingToResume() {
  return isWaitingToResume;
}

/**
 * This method will set the the thread as blocked.
 */
void Thread::blockThread() {
  tStatus = blocked;
  isWaitingToResume = true;
}

/**
 * This method will set the the thread as unblocked.
 */
void Thread::unblockThread() {
  isWaitingToResume = false;
}

/**
 * @return the current thread total running time in quantum time-unit.
 */
int Thread::getQuantumRunTime() {
  return quantumRunningCounter;
}

/**
 * @return the current thread total running time in quantum time-unit.
 */
void Thread::incrementQuantumRunTime() {
  quantumRunningCounter++;
}

/**
 * @param tid - the thread id to sync this thread to.
 */
void Thread::setSyncedTo(int tid) {
  syncedTo = tid;
}

/**
 * @return the tid of this thread is syncedTo. -1 if thread is NOT synced.
 */
int Thread::getSyncedTo() {
  return syncedTo;
}

/**
 * This method will add the given thread pointer to the dependants of this thread.
 * @param ptr thread pointer to insert as dependent.
 */
void Thread::addToDependants(Thread *&ptr) {
  dependantThreads.push_back(ptr);
}

/**
 * @return the thread dependants.
 */
std::deque<Thread *> *Thread::getThreadDependants() {
  return &dependantThreads;
}

/**
 *  This method will get a thread pointer and remove it's dependency from current thread.
 * @param thread Thread pointer to remove from thread dependents.
 */
void Thread::clearSyncDep(Thread *thread) {
  for (unsigned int i = 0; i < dependantThreads.size(); ++i) {
    if (dependantThreads[i] == thread) {
      dependantThreads.erase(dependantThreads.begin() + i);
      return;
    }
  }
}