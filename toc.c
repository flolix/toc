#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "help.h"
#include "osc.h"
#include "color.h"
#include "blobarray.h"
#include "udp.h"
#include "debug.h"
#include "token.h"


int RECEIVE_COLOR = GREEN;
int SEND_COLOR = NOCOLOR;
int DEBUG_COLOR = CYAN;


char IP[20] = "<no ip>";
int PORT = 0;

int OUT = 0b001;

void sendOSC(char * s) {
    char ip [20];
    int port;
    struct token_t tok; 

    tok = getnexttoken(tok, s); 
    strlcpy(ip, tok.chars, tok.len);

    tok = getnexttoken(tok, NULL);
    port = atoi(tok.chars);

    tok = getnexttoken(tok, NULL);
    s = tok.chars;

    struct OSCMsg_t * oscmsg;
    oscmsg = OSCcreateMessagefromstr(s);
    if (!OSCcheckaddress(oscmsg->addr)) {
        printf("Something wrong with your OSC message: ");
        OSCPrintMsg(oscmsg);
        puts("Probably a garbled address.");
        exit(1);
    }
    setcolor(SEND_COLOR);
    printf("<-- ");
    OSCPrintMsg(oscmsg);
    setcolor(NOCOLOR);
    //OUT:
    // Bit 3: print to stdout, Bit 2: print in hex to stdout, Bit 1: send via udp

    if ((OUT & 1) == 1) sendUDP(oscmsg->buf, oscmsg->len, ip, port);
    if ((OUT & 2) == 2) {
        int i;
        for (i = 0; i< oscmsg->len; i++) printf("%.2x ", (uint8_t) oscmsg->buf[i]); 
        puts("");
    }
    if ((OUT & 4) == 4) {
        int i;
        for (i = 0; i< oscmsg->len; i++) putc(oscmsg->buf[i], stdout); 
        fflush(stdout);
    }
    OSCFreeMessage(oscmsg);
}

  
void printconfigfile(char * name) {
    FILE * datei;
    //char line[300];
    datei = fopen(name, "r");  
    if (datei == NULL) {
        perror("open config file failed");
        exit(1);
    }   
    printf("Config file is %s\n", name);
    #define BUFSIZE 1024
    char * inbuf = malloc(BUFSIZE);
    int rb;
    while ((rb = fread(inbuf, 1, BUFSIZE, datei))) fwrite(inbuf, 1, rb, stdout); 

/*
    int i = 1;
    while( fgets(line,299,datei)) {
        removeTrailingNewlineChars(line);
        if ((line[0] == '/' && line[1] == '/') ||
            (line[0] == '#')) setcolor(D_GRAY);
        printf("%s\n", line);
        setcolor(NOCOLOR);
        i++;
    }
*/
    fclose(datei);
}

struct alias_t {
    char * s;
    char * r;
};

struct blob_t * AliasArray = NULL;

struct alias_t *  defineAlias(char * search, char * replace) {
    debprintf("New alias definition: search for %s -> replace by %s\n", search, replace);
    struct alias_t * newalias = malloc(sizeof(struct alias_t));
    newalias->s = malloc(strlen(search)+1);
    strcpy(newalias->s, search);
    newalias->r = malloc(strlen(replace)+1);
    strcpy(newalias->r, replace);
    appendBlob(&AliasArray, newalias);
    return newalias;
}
void applyAlias(struct alias_t * a, char * result, char * work) {
    result[0] = '\0';
    char * s;
    char * resptr = result;
    while ((s = strstr(work, a->s)) != NULL) {
        debprintf("Found %s in %s!\n", a->s, work);
        //printf("applyAlias: Firstpart is: %i\n", s-work);
        strncpy(resptr, work, s - work);
        strcpy(resptr+(s-work), a->r);
        //printf("applyAlias: with repl: %s\n", result);
        //setzt resptr ans ende.. man könnte auch resptr = result+strlen(result) schreiben
        resptr = strchr(resptr, '\0');
        work = s+strlen(a->s);
    }
   strcat(result, work);
}


void  applyAliase (char * result, char * line) {
    //printf("applyAliase: Working on line %s\n", line);
    struct alias_t * a;
    char  between[1000];
    strcpy(between, line);
    prepBlobIteration(AliasArray);
    while ((a = getNextBlob(AliasArray)) != NULL)  {
        applyAlias(a, result, between);
        strcpy(between, result); 
    } 
    strcpy(result, between);
}

void  applyAliaseNonRepeated (char * result, char * line) {
    debprintf("applyAliaseNonRepeated: Working on line >%s<\n", line);
    struct alias_t * a;
    char * work = line;
    result[0] = '\0';
    prepBlobIteration(AliasArray);
        char * resptr = result;
    while ((a = getNextBlob(AliasArray)) != NULL)  {
        char * s;
        while ((s = strstr(work, a->s)) != NULL) {
            debprintf("%s found, at %s\n", a->s,s );
            strncpy(resptr, work, s - work); // first part
            strcpy(resptr + (s-work), a->r); // replacement
            work = s + strlen(a->s);
            resptr = resptr + (s - work) + strlen(a->r)+2;
        }
    } 
    strcat(result, work);
    debprintf("After: applyAliaseNonRepeated %s\n", result);
}

struct line_t {
    int linenumber;
    int com;
    char * str;
};

struct blob_t * LineArray = NULL;

void addline(struct token_t * tok, char * line) {
    struct line_t * newline = malloc(sizeof(struct line_t));
    newline->str = malloc(strlen(line)+1);
    strcpy(newline->str, line);
    newline->com = tok->comm;
    if (getLastBlob(LineArray) == NULL)  newline->linenumber = 1;
    else newline->linenumber = ((struct line_t *) getLastBlob(LineArray))->linenumber+1; 
    appendBlob(&LineArray, newline);
}

struct line_t * getLine(int i) {
    return getBlob(LineArray, i-1);
}

struct blob_t * ConfigFile = NULL;

char * getconfigline() {
    return getNextBlob(ConfigFile);
}

void addconfigline(char * line) {
    char * newline = malloc(strlen(line)+1);
    strcpy(newline, line);
    appendBlob(&ConfigFile, newline);
}

void makeConfigPath(char * configfile) {
    //find the path of the config file by
    // using the system which tool
    FILE *which;
    which = popen ("which toc", "r");
    fgets (configfile, 99, which); 
    pclose(which);

    //ok got the path and filename of the binary .. now removing the last \n    
    removeTrailingNewlineChars(configfile);
    //and adding .config
    strcat(configfile, ".config");
    debprintf("Config file is: %s\n", configfile); 
}

pthread_t th;
struct fredparams_t params;

void startListen(char * ip, int port) {
    strcpy(params.ip, ip);
    params.port = port;
    if (pthread_create(&th, NULL, &listenfred, &params) != 0) {
        printf("Couldnt create thread\n");
        exit(EXIT_FAILURE);
    } 
} 

void printAliasTable() {
    int i = 1;
    puts("-----------");
    puts("Alias table");
    prepBlobIteration(AliasArray);
    struct alias_t * a;
    while ((a = getNextBlob(AliasArray)) != NULL) {
        printf("%i: %s -> %s\n",i, a->s, a->r);
        i++;
    }
    puts("-----------");
}

void searchAndReplaceEmptyToken(char * result, char * et) {
    debprintf("searchAndReplaceEmptyToken launched with >%s<\n", result);
    struct token_t tok;
    int c = NO_COM;
    int argc = 0;
    char * ptr = result;
    while(tok = getnexttoken(tok, ptr), tok.comm != TOK_END) {
        //debprintf("tok ist %s, tok.len = %d, tok.comm = %d\n", tok.str, tok.len, tok.comm);
        ptr = NULL;
        if (argc == 0) c = NO_COM; else argc--;

        //if (tok.comm != NO_COM && tok.comm != c) {
        //    printf("Neues Token %d\n", tok.comm);    
        //}  
 
        if (tok.comm != NO_COM) {c = tok.comm; argc = tokenlist[tok.comm].argc;}
        if (tok.comm == NO_COM && c == NO_COM) {
            debprintf("undefined token..%s, prepend %s\n", tok.str, et);
            //char * emptytoken_withspace;
            //emptytoken_withspace = malloc(strlen(et)+2);
            //strcpy(emptytoken_withspace, et); strcat(emptytoken_withspace, " ");
            //strinsert(tok.chars, emptytoken_withspace);
            //free(emptytoken_withspace);
            memmove(tok.chars + strlen(et) + 1, tok.chars, strlen(tok.chars) + 1);
            strcpy(tok.chars, et);
            tok.chars[strlen(et)] = ' ';
            ptr = tok.chars;
        }
    }
    debprintf("After SearchAndReplaceEmptyToken >%s<\n", result);
}

void printScript() {
    struct line_t * li;
    prepBlobIteration(LineArray);
    while((li = getNextBlob(LineArray)) != NULL)  {
        printf("%i: %5s, %s\n", li->linenumber, tokenlist[li->com].str, li->str);
    }
}

int main (int argc, char ** argv) {
    char configfile[100] = "";
    makeConfigPath(configfile);
    //evaluating commandline arguments.. there are some special commands like
    //concatenate the rest to one chunk
    bool printscript = false;
    bool listen = false;
    char commandline [200] = "";
    bool printaliastable = false;
    if (argc == 1) printusage();
    int i;
    for (i = 1; i< argc; i++) {
        if (strcmp(argv[i], "-?") == 0) {
            printhelp();
            exit(0);
        } else if (strcmp(argv[i], "config") == 0) {
            printconfigfile(configfile);
            exit(0);
        } else if (strcmp(argv[i], "-d") == 0) {
            DEBUG = 1;
            debprintf("DEBUG messages activated\n");
            continue;
         } else if (strcmp(argv[i], "script") == 0) {
            printscript = true;
            continue;
        } 
        //} else if (strcmp(argv[i] , "listen") == 0) {
           // if (pthread_create(&th, NULL, &listenfred, NULL) != 0) {
           //     printf("Couldnt create thread\n");
           //     exit(EXIT_FAILURE);
           // } 
           // listen = true;
           // continue;
        strcat(commandline, argv[i]);
        strcat(commandline, " ");
    };


    FILE * datei;
    struct alias_t * EMP_AL = NULL;
    datei = fopen(configfile, "r");
    if (datei == NULL) 
        puts("The config file does not exist. Please refer to 'toc -?' if you dont know what I am writing about..");
    else {
        debputs("Processing configfile");
 //loadfile
        fseek(datei, 0, SEEK_END);
        int filesize;
        filesize = ftell(datei);
        char * buf;
        buf = malloc(filesize+2);
        fseek(datei,0,SEEK_SET);
        fread(buf, filesize, 1,datei);
        //trailing \n and \0,
        buf[filesize] = '\n';
        buf[filesize+1] = '\0';
        char * nl;
        nl = buf-1;
        char * ptr;
        while ((ptr = nl+1), *ptr != '\0') {
            nl = strchr(ptr, '\n');
            if (nl != NULL) *nl = '\0'; 
            if (ptr[0] == '/' && ptr[1] == '/') continue;
            if (ptr[0] == '#') continue;
            if (ptr[0] == '\n') continue;
            char result[1000];
            removeTrailingNewlineChars(ptr);
            applyAliase(result, ptr); 
            char * ap;
            if ((ap = strstr(result, ":= ")) != NULL) {
                *ap = '\0';
                removeTrailingSpaceChars(result);
                if (result[0] == '\0') EMP_AL = defineAlias("EMPTY_TOKEN", ap +3); 
                else defineAlias(result, ap+3);
                continue;
            } 
            if (EMP_AL != NULL) searchAndReplaceEmptyToken(result, EMP_AL->r);
            addconfigline(result);
        } 
        debputs(" ** Config file has been successfully processed. ** ");
        fclose(datei);
        free(buf);
    } 

/*
        char line[200];
        while (fgets(line, 200, datei)) {
            if (line[0] == '/' && line[1] == '/') continue;
            if (line[0] == '#') continue;
            if (line[0] == '\n') continue;
            char result[1000];
            removeTrailingNewlineChars(line);
            applyAliase(result, line);
            //applyAliaseNonRepeated(result, line);
            //now look for new alias definitios
            char * ap;
            if ((ap = strstr(result, ":= ")) != NULL) {
                *ap = '\0';
                removeTrailingSpaceChars(result);
                if (result[0] == '\0') EMP_AL = defineAlias("EMPTY_TOKEN", ap +3); 
                else defineAlias(result, ap+3);
                continue;
            } 
            if (EMP_AL != NULL) searchAndReplaceEmptyToken(result, EMP_AL->r);
            addconfigline(result);
        } 
        debputs(" ** Config file has been successfully processed. ** ");
        fclose(datei);
    }
*/
    if (printaliastable) printAliasTable();

    debprintf("Processing commandline: %s\n", commandline);
    char result[200];
    applyAliaseNonRepeated(result, commandline);
    if (EMP_AL != NULL) searchAndReplaceEmptyToken(result, EMP_AL->r);
    addconfigline(result);
    debputs(" ** Commandline has been successfully processed. ** ");

    debputs("Parsing.. ");
    //now parse everything
    char * currentline = NULL;
    prepBlobIteration(ConfigFile);
    while ((currentline = getconfigline()) != NULL) {
        struct token_t tok;
        struct token_t ctok;
        ctok.comm = NO_COM;
        char l[50] ="";
        while (tok = getnexttoken(tok, currentline), tok.comm != TOK_END) {
            currentline = NULL;
            debprintf("current token is : >%s<, with length %d, and com %d, type %d\n", tok.str , tok.len, tok.comm, tok.type);
            struct token_t seektok = getnexttoken(tok, NULL);
            //debprintf("and seek token is : >%s<, with length %d, and type %d\n", seektok.str , seektok.len, seektok.type);
            fflush(stdout);

            if (tok.comm == TOK_EOL) continue; // an empty line

            if (tok.comm == OSC_COM) {
                ctok = tok;
                l[0] = '\0';
                continue;
            } else if (tok.comm == PAUSE_COM) {
                ctok = tok;
                tok = getnexttoken(tok, NULL);
                addline(&ctok, tok.str);
                continue;
           } else if (tok.comm == OUT_COM) {
                ctok = tok;
                tok = getnexttoken(tok, NULL);
                addline(&ctok, tok.str); 
                continue;
            } else if (tok.comm == LOOP_COM) {
                addline(&tok, "");
                continue;
            } else if (tok.comm == ECHO_COM) {
                ctok = tok;
                l[0] = '\0';
                continue;
            } else if (tok.comm == LISTEN_COM) {
                char lip[20];
                int lport;
                tok = getnexttoken(tok, NULL);
                strlcpy(lip, tok.chars, tok.len);
                tok = getnexttoken(tok, NULL);
                lport = atoi(tok.chars);
                printf("StartListen with %s, %i\n", lip, lport);
                startListen(lip, lport); 
                listen = true;
               continue;
            }   else if (ctok.comm == NO_COM) {
                    printf("Dont know what to do with %s\n", tok.chars);
                    printf("May be there is an '-o' missing or look for 'EMPTY TOKEN' in the man pages\n");
                    exit(0);
            }

            strncat(l, tok.chars, tok.len); strcat(l, " ");

            if (seektok.comm == TOK_END || seektok.comm != NO_COM ) {
                removeTrailingSpaceChars(l);
                addline(&ctok, l);
                l[0] = '\0';
                ctok.comm  = NO_COM;
                continue;
            }

        }
    }
        

    //print script..
    if (printscript || DEBUG)  printScript();  

    //run script
    debprintf("--------------------\n");
    debprintf("Now run the script..\n");
    if (LineArray == NULL) debputs("There is none..");
    if (LineArray != NULL) { 
        struct line_t * cl;      //current line
        int linenumber = 1;
        cl = getLine(linenumber);
        do  {
            debprintf("Line %i :", linenumber);
            debprintf(" current command is : %i, with line %s length %li\n", cl->com , cl->str, strlen(cl->str));

            if (cl->com == OSC_COM) {
                debprintf("send osc message\n");
                sendOSC(cl->str);
            } else if (cl->com == PAUSE_COM) {
                usleep(atoi(cl->str) * 1000);
            } else if (cl->com == LOOP_COM) {
                linenumber = 0;
            } else if (cl->com == ECHO_COM) {
                printf("%s\n", cl->str);
            } else if (cl->com == OUT_COM) {
                if (strcmp(cl->str,"stdout") == 0) { OUT |= 0b100; OUT &= 0b110;}
                else if (strcmp(cl->str, "network")== 0) { OUT |= 0b001; OUT &=0b011;}
            }    
            linenumber++;
            cl = getLine(linenumber);
        }  while (cl != NULL);
        removeBlobArray(LineArray);
    }

    if (listen) pthread_join(th , NULL);
}

