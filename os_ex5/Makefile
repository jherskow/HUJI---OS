CC = g++
CXX = g++

INCS=-I.
CFLAGS = -Wall -std=c++11 -g $(INCS)
CXXFLAGS = -Wall -pthread -std=c++11 -g $(INCS)


TAR = tar
TARFLAGS = -cvf
TARNAME = ex3.tar
TARSRCS = MapReduceFramework.cpp Makefile README

default: libMapReduceFramework.a

libMapReduceFramework.a: MapReduceFramework.o
	ar rcs $@ $^
	ranlib $@

t: main

main: main.o libMapReduceFramework.a

.PHONY : clean
clean:
	$(RM) *.o libMapReduceFramework.a main $(TARNAME) *~

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)