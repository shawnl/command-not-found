CC = cc
CFLAGS = -Wall -g -O2

default: command-not-found pts_lbsearch

rb.o:
	$(CC) rb.c -o rb.o -c $(CFLAGS)

command-not-found: pts_lbsearch
	$(CC) command-not-found.c -o command-not-found $(CFLAGS)

pts_lbsearch:
	$(CC) pts_lbsearch.c -o pts_lbsearch $(CFLAGS)

clean:
	-rm command-not-found rb.o pts_lbsearch
