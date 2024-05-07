CC = gcc
CFLAGS = -Wall -I.. -ggdb
LDFLAGS = -lpthread

all: test_tracer

test_tracer: test_tracer.o tracer.o
	$(CC) $(CFLAGS) -o test_tracer test_tracer.o tracer.o $(LDFLAGS)

clean:
	rm -f test_tracer *.o
