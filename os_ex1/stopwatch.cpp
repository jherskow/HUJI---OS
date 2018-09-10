#include "osm.h"
#include <iostream>
#include <fstream>

double time_of_operation(unsigned int iterations) {
    osm_init();
    double result = osm_operation_time(iterations);
    osm_finalizer();
    return result;
}


double time_of_function(unsigned int iterations) {
    osm_init();
    double result = osm_function_time(iterations);
    osm_finalizer();
    return result;
}


double time_of_syscall(unsigned int iterations) {
    osm_init();
    double result = osm_syscall_time(iterations);
    osm_finalizer();
    return result;
}


int main() {
    // Auto Setting of iters
//    unsigned int iters = 1000;
//    std::cout << "Auto mode of iterations settings" << std::endl;
//    unsigned int iters = 1000000;
//    std::cout << "Auto mode of iterations set to MILLION" << std::endl;

    // Manual setting of iters
    // If using need to turn on back redirection at the bottom
    unsigned int iters;
    std::cout << "Enter num of iters (0 - default value): " << std::endl;
    std::cin >> iters;

    int times = 1;

    // Output to file.
//    std::ofstream outfile("results.txt");
//    std::streambuf * cout_std_buf = std::cout.rdbuf(); //save old buf
//    std::cout.rdbuf(outfile.rdbuf()); //redirect std::cout to out.txt!


    for (int i=0 ; i< times; i++) {
        double operation = time_of_operation(iters);
        if (operation == -1) {
            std::cout << "Error in Operation time" << std::endl;
        } else {
            std::cout << "osm_operation_time result: " << operation << std::endl;
        }
    }

    for (int i=0 ; i< times; i++) {
        double function = time_of_function(iters);
        if (function == -1) {
            std::cout << "Error in Function time" << std::endl;
        } else {
            std::cout << "osm_function_time result: " << function << std::endl;
        }
    }

    for (int i=0 ; i< times; i++) {
        double sysCall = time_of_syscall(iters);
        if (sysCall == -1) {
            std::cout << "Error in SystemCall time" << std::endl;
        } else {
            std::cout << "osm_syscall_time result: " << sysCall << std::endl;
        }
    }

    // turning Output to screen again.
//    std::cout.rdbuf(cout_std_buf); //redirect std::cout to original cout.
//    outfile.close();
}
