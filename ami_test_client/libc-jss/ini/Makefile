CFLAGS = -Wall

all: test_ini

test_ini: ini.o test_ini.o
	gcc -o test_ini $(CFLAGS) test_ini.o ini.o

clean:
	rm -f test_ini *.o
