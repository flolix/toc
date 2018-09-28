EXECDIR = ~/bin
CC = gcc
CFLAGS = -Wall
LIBS = -lpthread

CFILES = $(wildcard *.c)

toc: $(CFILES)
	$(CC) $(CFLAGS) -o toc $(CFILES) $(LIBS)

copy:
	cp toc.config $(EXECDIR)/toc.config

install: copy
	cp toc $(EXECDIR)/toc

.PHONY: copy
