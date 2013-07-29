LDFLAGS=-lsqlite3 -lfuse
CFLAGS=-D_FILE_OFFSET_BITS=64 -Wall
CC=cc

all: fusedocs

run: clean all
	mkdir -p mountpoint 
	./fusedocs -d mountpoint

clean:
	rm -f *.o /tmp/test.db fusedocs

fusedocs: fusedocs.o sql.o 
	$(CC) -o fusedocs sql.o fusedocs.o $(LDFLAGS)
