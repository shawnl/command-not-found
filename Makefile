CC = cc
CFLAGS = -Werror -g -O2

default: update-command-not-found command-not-found pts_lbsearch

rb.o:
	$(CC) rb.c -o rb.o -c $(CFLAGS)

update-command-not-found: rb.o
	$(CC) update-command-not-found.c -o update-command-not-found rb.o $(CFLAGS)

command-not-found: pts_lbsearch
	$(CC) command-not-found.c -o command-not-found $(CFLAGS)

pts_lbsearch:
	$(CC) pts_lbsearch.c -o pts_lbsearch $(CFLAGS)

clean:
	-rm command-not-found update-command-not-found rb.o pts_lbsearch
