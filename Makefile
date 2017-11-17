CC = cc
CFLAGS = -Wall -g -O0

default: command-not-found

command-not-found: pts_lbsearch.o util.o
	$(CC) command-not-found.c -o command-not-found $(CFLAGS) pts_lbsearch.o util.o -lkyotocabinet

pts_lbsearch.o:
	$(CC) pts_lbsearch.c -c -o pts_lbsearch.o $(CFLAGS)

util.o:
	$(CC)  util.c -c -o util.o $(CFLAGS)

clean:
	-rm command-not-found pts_lbsearch.o util.o
