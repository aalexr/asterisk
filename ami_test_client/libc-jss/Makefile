
CC = gcc
CFLAGS = -Wall
LIBS = -lcrypto -lev
OBJ=pipe.o netsocket.o
BIN=pipe

all: logger pipe fmtsub_test

pipe: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(EXTOBJ) $(LIBS)

logger: logger.o logger_test.o misc.o
	$(CC) $(CFLAGS) -o logger logger.o logger_test.o misc.o

fmtsub_test: misc.o fmtsub_test.o
	$(CC) $(CFLAGS) -o fmtsub_test misc.o fmtsub_test.o

doc:
	doxygen
	@echo ""
	@echo "DOC URL: file://"`pwd`"/html/index.html"
	@echo ""

clean:
	rm -rf $(OBJ) $(BIN) logger fmtsub_test core *.o html/ latex/ man/

# Függőségek
#policy.o: server.h
