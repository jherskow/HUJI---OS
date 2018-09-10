CC = g++
CXX = g++

INCS=-I.
CFLAGS = -Wall -std=c++11 -g $(INCS)
CXXFLAGS = -Wall -pthread -std=c++11 -g $(INCS)


TAR = tar
TARFLAGS = -cvf
TARNAME = ex3.tar
TARSRCS = MapReduceFramework.cpp Makefile README Barrier.cpp Barrier.h

default: libMapReduceFramework.a

libMapReduceFramework.a: MapReduceFramework.o Barrier.o
	ar rcs $@ $^

.PHONY : clean
clean:
	$(RM) *.o libMapReduceFramework.a main $(TARNAME) *~

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)