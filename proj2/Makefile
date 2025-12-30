all: problem1 problem2  

CC = g++
CFLAGS = -O2 -fopenmp

problem1: problem1.cpp
	$(CC) $(CFLAGS) -o problem1 $<

problem2: problem2.cpp
	$(CC) $(CFLAGS) -o problem2 $<

clean:
	rm -f problem1 problem2  
