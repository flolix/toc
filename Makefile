~/bin/toc: toc.c osc.c help.c color.c blobarray.c udp.c
	gcc -o ~/bin/toc toc.c osc.c help.c color.c blobarray.c udp.c -Wall -lpthread
