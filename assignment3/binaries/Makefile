CC=gcc
CFLAGS=-Wall -Iincludes -Wextra -std=c99 -ggdb
LDLIBS=-lcrypto
VPATH=src

all: rclient


rclient: rclient.o

clean:
	rm -rf *~ *.o rclient


.PHONY : clean all
