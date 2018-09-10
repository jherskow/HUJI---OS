#include "osm.h"
#include <sys/time.h>

unsigned int numIters; // 1k default as specified
int UNROLL_FACTOR = 10;

const double MILLION = 1000000.0;
const double THOUSAND = 1000.0;
const unsigned int  DEFAULT_NUM_ITERS = 1000;

// forward declarations
void setup_iteration_number(unsigned int iterations);
double getTimeInterval(timeval &timeBefore, timeval &timeAfter);
void emptyFuncCall();


/* Initialization function that the user must call
 * before running any other library function.
 * The function may, for example, allocate memory or
 * create/open files.
 * Pay attention: this function may be empty for some desings. It's fine.
 * Returns 0 uppon success and -1 on failure
 */
int osm_init(){
    return 0;
}


/* finalizer function that the user must call
 * after running any other library function.
 * The function may, for example, free memory or
 * close/delete files.
 * Returns 0 uppon success and -1 on failure
 */
int osm_finalizer(){
    return 0;
}


/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_operation_time(unsigned int iterations){

    setup_iteration_number(iterations);

    // pre-stuff
    int x0,x1,x2,x3,x4,x5,x6,x7,x8,x9;
    x0=x1=x2=x3=x4=x5=x6=x7=x8=x9=0; //initialise vars

    // start time
    timeval timeBefore{}, timeAfter{};
    if (gettimeofday(&timeBefore, nullptr) != 0){
        return -1;
    }

    // loop
    for (unsigned int i = 0; i < numIters ; i+=UNROLL_FACTOR)
    {
        x0 += 1;  //1
        x1 += 1;  //2
        x2 += 1;  //3
        x3 += 1;  //4
        x4 += 1;  //5
        x5 += 1;  //6
        x6 += 1;  //7
        x7 += 1;  //8
        x8 += 1;  //9
        x9 += 1;  //10

    }

    // end
    if (gettimeofday(&timeAfter, nullptr) != 0){
        return -1;
    }

    //return the time difference
    return getTimeInterval(timeBefore, timeAfter);

}


/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_function_time(unsigned int iterations){

    setup_iteration_number(iterations);

    // start time
    timeval timeBefore{}, timeAfter{};
    if (gettimeofday(&timeBefore, nullptr) != 0){
        return -1;
    }

    for (unsigned int i = 0; i < numIters ; i+=UNROLL_FACTOR)
    {
        emptyFuncCall(); //1
        emptyFuncCall(); //2
        emptyFuncCall(); //3
        emptyFuncCall(); //4
        emptyFuncCall(); //5
        emptyFuncCall(); //6
        emptyFuncCall(); //7
        emptyFuncCall(); //8
        emptyFuncCall(); //9
        emptyFuncCall(); //10
    }

    // end
    if (gettimeofday(&timeAfter, nullptr) != 0){
        return -1;
    }

    //return the time difference
    return getTimeInterval(timeBefore, timeAfter);

}


/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_syscall_time(unsigned int iterations){

    setup_iteration_number(iterations);

    // start time
    timeval timeBefore{}, timeAfter{};
    if (gettimeofday(&timeBefore, nullptr) != 0){
        return -1;
    }
    
    for (unsigned int i = 0; i < numIters ; i+=UNROLL_FACTOR)
    {
        OSM_NULLSYSCALL; //1
        OSM_NULLSYSCALL; //2
        OSM_NULLSYSCALL; //3
        OSM_NULLSYSCALL; //4
        OSM_NULLSYSCALL; //5
        OSM_NULLSYSCALL; //6
        OSM_NULLSYSCALL; //7
        OSM_NULLSYSCALL; //8
        OSM_NULLSYSCALL; //9
        OSM_NULLSYSCALL; //10
    }

    // end
    if (gettimeofday(&timeAfter, nullptr) != 0){
        return -1;
    }

    //return the time difference
    return getTimeInterval(timeBefore, timeAfter);

}

/* rounds the iteration number, if necessary.
   yields default iterations, if necessary.
   */
void setup_iteration_number(unsigned int iterations){

    numIters = (iterations == 0) ? DEFAULT_NUM_ITERS : iterations;
    
    // round up iterations to match unrolling factor
    while (numIters % UNROLL_FACTOR != 0){
        numIters ++;
    }
    
}

/* Returns the time interval
   */
double getTimeInterval(timeval &timeBefore, timeval &timeAfter){
    double diffSeconds = timeAfter.tv_sec - timeBefore.tv_sec;

    double diffMicroSeconds = timeAfter.tv_usec - timeBefore.tv_usec;

    // add both units for total microseconds
    double totalMicro = (diffSeconds*MILLION) + diffMicroSeconds;

    // convert to nanoseconds, and return
    double totalNano = THOUSAND * totalMicro;

    double avgTime = totalNano / (double) numIters;

    return avgTime;
}

/* Empty Function Call
   */
void emptyFuncCall(){
}
