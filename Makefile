CC = gcc
RM = rm -f

.PHONY=all
all: tftp

tftp: client.c
	$(CC) -o $@ $^

.PHONY=clean
clean:
	$(RM) tftp
