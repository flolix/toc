~/bin/toc: toc.c osc.c help.c color.c blobarray.c udp.c debug.c token.c
	gcc -o ~/bin/toc toc.c osc.c help.c color.c blobarray.c udp.c debug.c token.c -Wall -lpthread
