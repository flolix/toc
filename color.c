#include <stdio.h>

char colstr[14][5] = {"0", "0;30", "0;31", "0;32", "0;33", "", "", "", "", "", "", "", "1;33"};

void setcolor(int col) {
    printf("\033[%sm", colstr[col]);    
}


