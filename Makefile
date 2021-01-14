CFLAGS = -g -std=gnu99
VFLAGS = --leak-check=full --track-origins=yes -v --show-leak-kinds=all

all: clean client server

%.o:%.c
	gcc $(CFLAGS) -c $^

client: 
	client.o pgmread.o send_packet.o linkedlist.o
	gcc $(CFLAGS) client.o pgmread.o send_packet.o linkedlist.o -o client

server: 
	server.o pgmread.o
	gcc $(CFLAGS) server.o pgmread.o -o server

clean:
	rm -f client server *.o

check:
	./client 127.0.0.1 1312 list_of_filenames.txt 0

checkloss:
	./client 127.0.0.1 1312 list_of_filenames.txt 10

checkserver:
	./server 1312 big_set results.txt

valgrind:
	valgrind $(VFLAGS) ./client 127.0.0.1 1312 list_of_filenames.txt 0

valgrindserver:
	valgrind $(VFLAGS) ./server 1312 big_set results.txt
