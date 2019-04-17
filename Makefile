CC = gcc
RM = rm -f
CFLAGS=-Wall -g -lpthread

PROG = tftp tftpd

.PHONY: all
all: $(PROG)

tftp: client.c
	$(CC) $(CFLAGS) -o $@ $^

tftpd: server.c
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	$(RM) $(PROG)
