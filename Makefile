CC=g++
LDFLAGS= --std=c++20

all: joy

joy: joy.cc
	$(CC) $(LDFLAGS) $(LIBS) joy.cc -o $@ -ggdb -g -O1 -lm

#test: tests.c
#	$(CC) $(LDFLAGS) $(LIBS) tests.c -o $@

clean:
	rm joy

.PHONY: all clean
