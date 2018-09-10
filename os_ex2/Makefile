CC = g++
CXX = g++

INCS=-I.
CFLAGS = -Wall -std=c++11 -g $(INCS)
CXXFLAGS = -Wall -std=c++11 -g $(INCS)


TAR = tar
TARFLAGS = -cvf
TARNAME = ex2.tar
TARSRCS = uthreads.cpp thread.h thread.cpp Makefile README

default: libuthreads.a

libuthreads.a: uthreads.o thread.o
	ar rcs $@ $^

t: main

main: main.o libuthreads.a

.PHONY : clean
clean:
	$(RM) *.o libuthreads.a main $(TARNAME) *~

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)