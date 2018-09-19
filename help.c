#include <stdio.h>
#include "color.h"

void printusage() {
    setcolor(YELLOW);
    printf("toc");
    setcolor(NOCOLOR);
    puts("  is a thin osc client, which formats and send OSC messages via UDP");
    puts("----------------------------------------------------------------------");
    puts("Invoke:  toc OSC-MESSAGE   or   toc DO-SCRIPT");
    puts("DO-SCRIPT is usually a chain of COMMAND VALUE pairs.");
    puts("Try 'toc -?'  or 'toc config'  for more information");
    puts("");
}

void printhelp() {
    setcolor(YELLOW);
    printf("toc");
    setcolor(NOCOLOR);
    puts("  is a thin osc client, which formats and send OSC messages via UDP");
    puts("----------------------------------------------------------------------");
    puts("Invoke:  toc OSC-MESSAGE   or   toc DO-SCRIPT");
    puts("");
    puts("DO-SCRIPT is usually a chain of COMMAND VALUE pairs. COMMAND could be");
    puts(" -o       .. VALUE is an OSC message");
    puts(" -p       .. pauses for VALUE millisecondes");
    puts(" -l       .. loops execution of DO-SCRIPT forever");
    puts(" -e       .. echo VALUE to stdout");
    puts(" -d       .. debug output");
    puts(" -ip      .. ip VALUE is ip address to send to");
    puts(" -port    .. VALUE is port number to send to");
    puts(" -out     .. VALUE could be: 'stdout', 'network'"); 
    puts(" -- COMMAND LINE ARGUMENTS ONLY --");
    puts(" script   .. print the script instead of execute it");
    puts(" config   .. prints the config file");
    puts(" show     .. shows you the formatted OSC message");
    puts(" -?       .. here you are now");
    puts("");
    puts("OSC commands");
    puts("------------");
    puts("/foo 42 test 3.14 will send an OSC message with an int, a string and a float.");
    puts("You could force a string by using quotation marks: /bar \"123\" ");
    puts("");
    puts("Config file");
    puts("-----------");
    puts("There is a general config file, toc.conf, which you should find in the folder of the binary itself");
    puts("You can use the same script commands in this file which can be used on the commandline, except of 'script',");
    puts("'config', 'stdout' and '-?'.");
    puts("However, there is one");
    puts("additional feature which makes your life easier. You can define aliases in the config file.");
    puts("Example: 'reset all -a /filter/0 -o /synth/0' will replace 'reset all' by everything after the '-a'.");
    puts("");
    puts("A sample 'toc.config' should come with this program. May be you have to move it to the binary folder");
    puts("if I am complainig about a missing file.");
    puts("");
    puts("Examples");
    puts("--------");
    puts("toc /1/midi 144 64 127   .. sends an OSC message with three ints ");
    puts("toc /filter lowpass 20   .. sends an OSC message with a string and one int");
    puts("toc /thatsfast -p 100 -l .. will send a message, wait for 100ms and repeat that forever");
    puts("toc -e Now it gets weird -o /shutup ");
    puts("                         .. will echo a message on the screen and send an OSC message");
    puts("toc -out stdout test 1 2 3 | xxd  .. will print the osc message in hex digits but not send it.");
    puts(""); 
    puts("Coded by Guistlerei"); 
} 
