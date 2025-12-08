CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

all: demo

demo: TinyFSApp.o TinyFS.o TinyDisk.o
	$(CC) $(CFLAGS) -o demo TinyFSApp.o TinyFS.o TinyDisk.o

TinyDisk.o: TinyDisk.c TinyDisk.h
	$(CC) $(CFLAGS) -c TinyDisk.c

TinyFS.o: TinyFS.c TinyFS.h TinyDisk.h
	$(CC) $(CFLAGS) -c TinyFS.c

TinyFSApp.o: TinyFSApp.c TinyFS.h TinyDisk.h
	$(CC) $(CFLAGS) -c TinyFSApp.c

clean:
	rm -f demo *.o
