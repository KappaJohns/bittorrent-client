CC = gcc
CFLAGS=-Wall -Iincludes -Wextra -std=c99 -ggdb -D_POSIX_C_SOURCE=200112L -D_DEFAULT_SOURCE
LDLIBS=-lcrypto

all: client

client: client.o client_parser.o bencode.o client_bencoding.o bigint.o hash.o send_receive.o peer_message.o bitfield.o stopwatch.o files.o
	$(CC) client.o client_parser.o bencode.o client_bencoding.o bigint.o hash.o send_receive.o peer_message.o bitfield.o stopwatch.o files.o -o client -lcrypto -lm

client.o: client.c client.h
	$(CC) -c $(CFLAGS) client.c

hash.o: hash.c hash.h
	$(CC) -c $(CFLAGS) hash.c

client_parser.o: client_parser.c client_parser.h
	$(CC) -c $(CFLAGS) client_parser.c

bencode.o: bencode.c bencode.h
	$(CC) -c $(CFLAGS) bencode.c

client_bencoding.o: client_bencoding.c client_bencoding.h
	$(CC) -c $(CFLAGS) client_bencoding.c

bigint.o: bigint.c bigint.h
	$(CC) -c $(CFLAGS) bigint.c

sendreceive.o: send_receive.c send_receive.h
	$(CC) -c $(CFLAGS) send_receive.c

bitfield.o: bitfield.c bitfield.h
	$(CC) -c $(CFLAGS) bitfield.c

peer_message.o: peer_message.c peer_message.h
	$(CC) -c $(CFLAGS) peer_message.c

stopwatch.o: stopwatch.c stopwatch.h
	$(CC) -c $(CFLAGS) stopwatch.c

files.o: files.c files.h
	$(CC) -c $(CFLAGS) files.c
clean:
	rm -rf *~ *.o client
rmfiles:
	rm -rf downloads

.PHONY : clean all