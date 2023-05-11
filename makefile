CC=gcc
CFLAGS=-Wall -Werror


all: stnc
stnc: stnc.c
	$(CC) $(CFLAGS) -o stnc stnc.c

clean:
	rm -f stnc *.o  *.txt