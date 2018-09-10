CC = g++
CXX = g++

INCS=-I.
CFLAGS = -Wall -std=c++11 -g $(INCS)
CXXFLAGS = -Wall -std=c++11 -g $(INCS)


TAR = tar
TARFLAGS = -cvf
TARNAME = ex4.tar
TARSRCS = ${ALL} Makefile README

CLIENT = whatsappClient.cpp whatsappio.h
SERVER = whatsappServer.cpp whatsappio.h
IO = whatsappio.cpp whatsappio.h
EXE = whatsappClient whatsappServer
ALL = ${IO} whatsappClient.cpp whatsappServer.cpp

default: all

all: ${EXE}

whatsappClient: whatsappClient.o whatsappio.o

whatsappServer: whatsappServer.o whatsappio.o

whatsappClient.o: ${CLIENT}

whatsappServer.o: ${SERVER}

whatsappio.o: ${IO}


.PHONY : clean
clean:
	$(RM) *.o  ${EXE} $(TARNAME) *~

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)