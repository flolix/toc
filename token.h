#ifndef FILE_TOKEN_H
#define FILE_TOKEN_H
enum {NO_COM, PAUSE_COM, OSC_COM, LOOP_COM, ECHO_COM, OUT_COM, LISTEN_COM, TOK_END, TOK_EOL, TOK_COMMENT};

struct token_t {
    char * chars;
    int len;
    int type;
    char str[40];
    int comm;
};

struct tokenlist_t {
    int com;
    char str[10];
    int argc;
};

extern struct tokenlist_t tokenlist[];
 
void removeTrailingNewlineChars(char * l);
void removeTrailingSpaceChars(char * l );
void strlcpy(char * dest, char * source, int len);
struct token_t getnexttoken(struct token_t tok, char * begin);

#endif
