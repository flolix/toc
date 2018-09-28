#include <stdlib.h>
#include <string.h>
#include "token.h"

struct tokenlist_t tokenlist[15] = {{NO_COM, "",0}, {PAUSE_COM, "-p", 1}, {OSC_COM, "-o", 99}, {LOOP_COM, "-l",0}, {ECHO_COM, "-e",99}, {OUT_COM,"-out",1},{LISTEN_COM, "listen",2},{TOK_END, "\0",0},{TOK_EOL, "\n",0},{TOK_COMMENT, "#", 1},{99}};


void removeTrailingNewlineChars(char * l) {
    //if (l[strlen(l)-1] == '\n') l[strlen(l)-1] = '\0';
    while(l[strlen(l)-1] == '\n') l[strlen(l)-1] = '\0';
}

void removeTrailingSpaceChars(char * l) {
    while(l[strlen(l)-1] == ' ') l[strlen(l)-1] = '\0';
}

#if (!defined __APPLE__) && (!defined HAVE_STRLCPY)
void strlcpy(char * dest, char * source, int len) {
    strncpy(dest, source, len-1);
    dest[len-1] = '\0';
}
#endif

struct token_t getnexttoken(struct token_t tok, char * begin) {
    struct token_t t;

    if (begin != NULL)  t.chars = begin; 
    else { t.chars = tok.chars + tok.len; }

    // if space (or spaces) then jump over
    while (*t.chars == ' ') t.chars++;

    if (*t.chars == '\0') {
        t.len = 0;
        t.comm = TOK_END;
        return t;
    }
    if (*t.chars == '\n') {
        t.len = 1;
        t.comm = TOK_EOL;
        return t;
    }

    char * eot = t.chars; 
    do { eot++;   } while (*eot != ' ' && *eot != '\n' && *eot != '\0');
    t.len =  eot - t.chars; 
    strlcpy(t.str, t.chars, t.len+1);
    int i = 0; 
    while (tokenlist[i].com != 99 && strcmp(tokenlist[i].str, t.str) != 0) i++; 
    if (tokenlist[i].com == 99) t.comm = NO_COM;
    else t.comm = tokenlist[i].com;
    return t;
}
