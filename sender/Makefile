CC = gcc
CFLAGS = -Wall -O2
LIBS = -lwiringPi -lm

all: send send_fast

send: send.c
	$(CC) $(CFLAGS) -o send send.c $(LIBS)

send_fast: send_fast.c
	$(CC) $(CFLAGS) -o send_fast send_fast.c $(LIBS)

clean:
	rm -f send send_fast
