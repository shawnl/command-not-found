CC = cc
CFLAGS = -Wall -g -O0

default: command-not-found pts_lbsearch.o

command-not-found: pts_lbsearch.o
	$(CC) command-not-found.c -o command-not-found $(CFLAGS) pts_lbsearch.o

pts_lbsearch.o:
	$(CC) pts_lbsearch.c -c -o pts_lbsearch.o $(CFLAGS)

clean:
	-rm command-not-found pts_lbsearch.o
