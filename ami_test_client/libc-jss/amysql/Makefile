CC = gcc
CFLAGS = -Wall -ggdb -I..
CFLAGS += -DCON_DEBUG

LIBS = -lev -lm -lmysqlclient -lpthread
OBJ=amysql.o ../logger.o ../misc.o

all: test_amysql test_sync_amysql

test_amysql: test_amysql.o $(OBJ)
	$(CC) $(CFLAGS) -o test_amysql test_amysql.o $(OBJ) $(LIBS)

test_sync_amysql: test_sync_amysql.o $(OBJ)
	$(CC) $(CFLAGS) -o test_sync_amysql test_sync_amysql.o $(OBJ) $(LIBS)

clean:
	rm -f $(OBJ) *.o test_amysql test_sync_amysql


