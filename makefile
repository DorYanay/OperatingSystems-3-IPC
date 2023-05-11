CC=gcc
CFLAGS=-Wall -Werror


all: stnc client
stnc: stnc.c
	$(CC) $(CFLAGS) -o stnc stnc.c
client: client.c
	$(CC) $(CFLAGS) -o client client.c
clean:
	rm -f stnc *.o  *.txt