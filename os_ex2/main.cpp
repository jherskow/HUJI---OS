#include <iostream>
#include "uthreads.h"

int main() {
  std::cout << "Running ok?" << std::endl;

  uthread_init(20000);
  uthread_spawn(0);

  std::cout << "Linkage Successful" << std::endl;

  return 0;
}