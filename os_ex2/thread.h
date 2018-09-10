
#ifndef OS_EX2_THREAD_H
#define OS_EX2_THREAD_H

#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <setjmp.h>
#include <queue>

enum ThreadStatus {
  ready, blocked, running
};

#define STACK_SIZE 4096 /* stack size per thread (in bytes) */
#define POST_JUMP 555 /* Signifies entry to saved point */
#define NOT_SYNCED (-1)

class Thread {
 private:
  int tid;
  int syncedTo;
  bool isWaitingToResume;
  int quantumRunningCounter;
  ThreadStatus tStatus;
  char tStack[STACK_SIZE];

  std::deque<Thread *> dependantThreads;

 public:
  /**
   * Constructor of Thread object.
   * @param tid the unique id of the new thread
   * @param f pointer to function
   */
  Thread(int tid, void (*f)(void));

  /**
   * Destructor of Thread object.
   */
  ~Thread();

  sigjmp_buf tEnv;

  //// tid
  int getTid();

  //// status
  ThreadStatus getStatus();

  void setStatus(ThreadStatus status);

  //// block
  void blockThread();

  void unblockThread();

  bool getIsWaitingToResume();

  //// sync
  void setSyncedTo(int tid);

  int getSyncedTo();

  void addToDependants(Thread *&ptr);

  std::deque<Thread *> *getThreadDependants();

  void clearSyncDep(Thread *thread);

  //// quant
  int getQuantumRunTime();

  void incrementQuantumRunTime();
};

#endif //OS_EX2_THREAD_H
