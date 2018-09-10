CC = g++
CXX = g++

INCS=-I.
CFLAGS = -Wall -std=c++11 -g $(INCS)
CXXFLAGS = -Wall -std=c++11 -g $(INCS)


TAR = tar
TARFLAGS = -cvf
TARNAME = ex1.tar
TARSRCS = osm.cpp Makefile README graph.png


all: libosm.a

stopwatch: stopwatch.o libosm.a
	$(CXX) $(CXXFLAGS) stopwatch.o libosm.a -o stopwatch

stopwatch.o: osm.h

libosm.a: osm.o
	ar rcs $@ $^

.PHONY : clean
clean:
	$(RM) *.o libosm.a $(TARNAME) *~

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
