#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#include "debug.h"
#include "color.h"


void debputs(char * s) {
    if (!DEBUG) return;
    setcolor(DEBUG_COLOR);
    setcolor(BOLD);
    puts(s);
    setcolor(NOCOLOR);
}

void debprintf(char * fs, ...) {
    if (!DEBUG) return;
    va_list argumente;
    va_start(argumente, fs);
    setcolor(DEBUG_COLOR);
    setcolor(BOLD);
    vprintf(fs, argumente);
    setcolor(NOCOLOR);
    va_end(argumente);
}

