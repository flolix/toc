#include <stdio.h>

char colstr[14][5] = {"0", "1", "30", "31", "32", "33", "34", "35", "36", "37"};

void setcolor(int col) {
    printf("\033[%sm", colstr[col]);    
}


