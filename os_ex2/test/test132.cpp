/**********************************************
 * Test 132: thread's signal mask is saved between switches (not including VTALRM)
 *
 * steps:
 * create three global sets of different signals (not including VTALRM) - set1, set2, set3
 * spawn threads 1,2,3
 * each thread K blocks setK and infinitely checks that his sigmask is setK
 *
 **********************************************/


#include <iostream>
#include <signal.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "uthreads.h"


#define GRN "\e[32m"
#define RED "\x1B[31m"
#define RESET "\x1B[0m"


#define NUM_THREADS 4

#define RUN 0
#define DONE 1

sigset_t set1, set2, set3;

char thread_status[NUM_THREADS];

void halt()
{
    while (true)
    {}
}


int next_thread()
{
    return (uthread_get_tid() + 1) % NUM_THREADS;
}

void check_sig_mask(const sigset_t& expected)
{
    for (unsigned int i = 0; i <= 20; i++)
    {
        sigset_t actual;
        sigprocmask(0, NULL, &actual);
//        std::cout << "check_sig_mask\n";
        if (memcmp(&expected, &actual, sizeof(sigset_t)) != 0)
        {
            printf(RED "ERROR - sigmask changed\n" RESET);
            exit(1);
        }

        // in the first 10 iterations let the thread stop because of sync / block.
        // in later iterations it will stop because of the timer
//        if (i < 5)
//        {
//            uthread_sync(next_thread());
//        }
//        else if (i < 10)
        if (i < 10)
        {
            std::cout << "i<10\n";
            uthread_block(uthread_get_tid());
        }
        else
        {
            std::cout << "i>=10\n";
            int quantum = uthread_get_quantums(uthread_get_tid());
            while (uthread_get_quantums(uthread_get_tid()) == quantum)
            {}
        }
    }
    thread_status[uthread_get_tid()] = DONE;
    std::cout << "csm about to halt\n";
    halt();
}

void thread1()
{
//    std::cout << "thread1\n";
    sigprocmask(SIG_BLOCK, &set1, NULL);
    check_sig_mask(set1);
}

void thread2()
{
//    std::cout << "thread2\n";
    sigprocmask(SIG_BLOCK, &set2, NULL);
    check_sig_mask(set2);
}

void thread3()
{
//    std::cout << "thread3\n";
    sigprocmask(SIG_BLOCK, &set3, NULL);
    check_sig_mask(set3);
}

bool all_done()
{
//    std::cout << "all_done\n";
    bool res = true;
    for (int i = 1; i < NUM_THREADS; i++)
    {
        std::cout << "i=" << i << "\n";
        res = res && (thread_status[i] == DONE);
    }
    std::cout << "res is:" << res << "\n";
    return res;
}

int main()
{
    printf(GRN "Test 132:  " RESET);
    fflush(stdout);

    sigemptyset(&set1);
    sigemptyset(&set2);
    sigemptyset(&set3);

    sigaddset(&set1, SIGBUS);
    sigaddset(&set1, SIGTERM);
    sigaddset(&set1, SIGRTMAX);
    sigaddset(&set1, SIGABRT);

    sigaddset(&set2, SIGUSR1);
    sigaddset(&set2, SIGSEGV);
    sigaddset(&set2, SIGUSR2);
    sigaddset(&set2, SIGPIPE);

    sigaddset(&set3, SIGTSTP);
    sigaddset(&set3, SIGTTIN);
    sigaddset(&set3, SIGTTOU);
    sigaddset(&set3, SIGBUS);

    uthread_init(50);

    for (int i = 1; i < NUM_THREADS; i++)
    {
        thread_status[i] = RUN;
    }

    int t1 = uthread_spawn(thread1);
    int t2 = uthread_spawn(thread2);
    int t3 = uthread_spawn(thread3);

    if (t1 == -1 || t2 == -1 || t3 == -1)
    {
        printf(RED "ERROR - threads spawning failed\n" RESET);
        exit(1);
    }


    int tid = 0;
    std::cout << "before !all_done loop\n";
    while (!all_done())
    {
        // resume all threads, as each one of them is blocking himself
        std::cout << "tid:" << tid <<"\n";
        uthread_resume(tid);
        tid = (tid + 1) % NUM_THREADS;
    }


    printf(GRN "SUCCESS\n" RESET);
    uthread_terminate(0);
}
