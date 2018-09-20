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


int RECEIVE_COLOR = GREEN;
int SEND_COLOR = NOCOLOR;
int DEBUG_COLOR = CYAN;


char IP[20] = "<no ip>";
int PORT = 0;

int OUT = 0b001;

void sendOSCfromstr(char * s) {
    char * endptr;
    endptr = strchr(s, ' ');
    *endptr = '\0';
    strcpy(IP, s);
    *endptr = ' ';
    s = endptr+1; 
    PORT = strtol(s, &endptr, 10);
    s = endptr+1;
    struct OSCMsg_t * oscmsg;
    oscmsg = OSCcreateMessagefromstr(s);
    if (!OSCcheckaddress(oscmsg->addr)) {
        printf("Something wrong with your OSC message:");
        OSCPrintMsg(oscmsg);
        puts("Probably the address");
        exit(1);
    }
    setcolor(SEND_COLOR);
    printf("<-- ");
    OSCPrintMsg(oscmsg);
    setcolor(NOCOLOR);
    //OUT:
    // Bit 3: print to stdout, Bit 2: print in hex to stdout, Bit 1: send via udp

    if ((OUT & 1) == 1) sendUDP(oscmsg->buf, oscmsg->len, IP, PORT);
    if ((OUT & 2) == 2) {
        int i;
        for (i = 0; i< oscmsg->len; i++) {
            printf("%.2x ", (uint8_t) oscmsg->buf[i]);
        }
        puts("");
    }
    if ((OUT & 4) == 4) {
        int i;
        for (i = 0; i< oscmsg->len; i++) {
            putc(oscmsg->buf[i], stdout);
        }
        fflush(stdout);
    }
    OSCFreeMessage(oscmsg);
}

struct token_t {
    char * chars;
    int len;
    int type;
    char str[40];
    int comm;
};

enum {NON, START, END, EOL, COM};

enum {NO_COM, PAUSE_COM, OSC_COM, LOOP_COM, ECHO_COM, OUT_COM, SCRIPT_COM, LISTEN_COM};
struct tokenlist_t {
    int com;
    char str[10];
    int argc;
} tokenlist[15] = {{NO_COM, "",0}, {PAUSE_COM, "-p", 1}, {OSC_COM, "-o", 99}, {LOOP_COM, "-l",0}, {ECHO_COM, "-e",99}, {OUT_COM,"-out",1},{SCRIPT_COM, "script",0},{LISTEN_COM, "listen",2},{99}};


struct token_t getnexttoken(struct token_t tok, char * begin) {
    struct token_t t;
    t.type = NON;
    t.len = 1;
    t.str[0] = '\0';
    t.chars = NULL;
    t.comm = NO_COM;

    if (begin != NULL) {
        t.chars = begin;
    } else {
        t.chars = tok.chars+tok.len;
        // if space (or spaces) then jump over
        while (*t.chars == ' ') t.chars++;
    }
    if (*t.chars == '\n') {
        t.type = EOL;
        return t;
    }
 
    if (*t.chars == '\0') {
        t.type = END;
        t.len = 0;
        return t;
    }
  
    char * eot = t.chars; 
    do {
        eot++;   
    } while (*eot != ' ' && *eot != '\n' && *eot != '\0');
 
    t.len =  eot - t.chars; 
    strncpy(t.str, t.chars, t.len);
    t.str[t.len] = '\0';
   
    int i = 0; 
    while (tokenlist[i].com != 99 && strcmp(t.str, tokenlist[i].str) != 0) i++;
    if (tokenlist[i].com == 99) t.comm = NO_COM;
    else t.comm = tokenlist[i].com;
    
    return t;
} 
  
void printconfigfile(char * name) {
    FILE * datei;
    char line[300];
    datei = fopen(name, "r");  
    if (datei == NULL) {
        perror("open config file failed");
        exit(1);
    }   
    int i = 1;
    printf("Config file is %s\n", name);
    //puts  ("");
    while( fgets(line,299,datei)) {
        //remove trailing newline if any..
        if (line[strlen(line)-1] == '\n') line[strlen(line)-1] = '\0';
        if ((line[0] == '/' && line[1] == '/') ||
            (line[0] == '#')) setcolor(D_GRAY);
        printf("%s\n", line);
        setcolor(NOCOLOR);
        i++;
    }
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
        //printf("applyAliase: Between %s\n", between);
    } 
    strcpy(result, between);
    //printf("applyAliase: result is %s\n", result);
}


void  applyAliaseNonRepeated (char * result, char * line) {
    debprintf("applyAliaseNonRepeated: Working on line >%s<\n", line);
    struct alias_t * a;
    char * work = line;
    result[0] = '\0';
    prepBlobIteration(AliasArray);
    while ((a = getNextBlob(AliasArray)) != NULL)  {
        char * resptr = result;
        char * s;
        while ((s = strstr(work, a->s)) != NULL) {
            debprintf("%s found, at %s\n", a->s,s );
            strncpy(resptr, work, s - work); // first part
            strcpy(resptr + (s-work), a->r); // replacement
            work = s + strlen(a->s);
            resptr = resptr + (s - work) + strlen(a->r);
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

void makingconfigpath(char * configfile) {
    //find the path of the config file by
    // using the system which tool
    FILE *which;
    which = popen ("which toc", "r");
    fgets (configfile, 99, which); 
    pclose(which);

    //ok got the path and filename of the binary .. now removing the last \n    
     configfile[strlen(configfile)-1] = '\0';
    //and adding .config
    strcat(configfile, ".config");
    if (DEBUG) printf("Config file is: %s\n", configfile); 
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
    int i;
    puts("-----------");
    puts("Alias table");
    prepBlobIteration(AliasArray);
    i = 1;
    struct alias_t * a;
    while ((a = getNextBlob(AliasArray)) != NULL) {
        printf("%i: %s -> %s\n",i, a->s, a->r);
        i++;
    }
    puts("-----------");
}

void searchAndReplaceEmptyToken(char * result, char * et) {
    struct token_t tok;
    int c = 0;
    int argc;
    char * ptr = result;
    while(tok = getnexttoken(tok, ptr), tok.type != END) {
        ptr = NULL;
        if (argc == 0) c = NO_COM;
        argc--;
        if (tok.comm != NO_COM) {c = tok.comm; argc = tokenlist[tok.comm].argc;}
        if (tok.comm == NO_COM && c == NO_COM) {
            debprintf("undefined token..%s, prepend %s\n", tok.str, et);
            memmove(tok.chars + strlen(et) + 1, tok.chars, strlen(tok.chars) + 1);
            strcpy(tok.chars, et);
            tok.chars[strlen(et)] = ' ';
            ptr = tok.chars;
        }
    }
    debprintf("After SearchAndReplace.. %s\n", result);
}

void removeTrailingNewlineChar(char * line) {
    if (line[strlen(line)-1] == '\n') line[strlen(line)-1] = '\0';
}

void removeTrailingSpaces(char * l) {
    while(l[strlen(l)-1] == ' ') l[strlen(l)-1] = '\0';
}


int main (int argc, char ** argv) {
    char configfile[100] = "";
    makingconfigpath(configfile);
    //evaluating commandline arguments.. there are some special commands like
    //concatenate the rest to one chunk
    bool run = true;
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
        /* } else if (strcmp(argv[i], "script") == 0) {
            printscript = true;
            run = false;
            continue;
        */
        //} else if (strcmp(argv[i] , "listen") == 0) {
           // if (pthread_create(&th, NULL, &listenfred, NULL) != 0) {
           //     printf("Couldnt create thread\n");
           //     exit(EXIT_FAILURE);
           // } 
           // listen = true;
           // continue;
        } else if (strcmp(argv[i], "alias" ) == 0) {
            printaliastable = true;
            continue;
        }
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
        //raed it..
        char line[200];
        while (fgets(line, 200, datei)) {
            if (line[0] == '/' && line[1] == '/') continue;
            if (line[0] == '#') continue;
            if (line[0] == '\n') continue;
            char result[1000];
            removeTrailingNewlineChar(line);
            applyAliase(result, line);
            //applyAliaseNonRepeated(result, line);
            //now look for new alias definitios
            char * ap;
            if ((ap = strstr(result, ":= ")) != NULL) {
                *ap = '\0';
                while(result[strlen(result)-1] == ' ') result[strlen(result)-1] = '\0';
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
        while (tok = getnexttoken(tok, currentline), tok.type != END) {
            currentline = NULL;
            debprintf("current token is : >%s<, with length %d, and com %d, type %d\n", tok.str , tok.len, tok.comm, tok.type);
            struct token_t seektok = getnexttoken(tok, NULL);
            //debprintf("and seek token is : >%s<, with length %d, and type %d\n", seektok.str , seektok.len, seektok.type);
            fflush(stdout);

            if (tok.type == EOL) continue; // an empty line

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
            } else if (tok.comm == SCRIPT_COM) {
                addline(&tok, "");
                continue;
            } else if (tok.comm == LISTEN_COM) {
                char lip[20];
                int lport;
                tok = getnexttoken(tok, NULL);
                strcpy(lip, tok.str);
                tok = getnexttoken(tok, NULL);
                lport = atoi(tok.str);
                printf("StartListen with %s, %i\n", lip, lport);
                startListen(lip, lport); 
                listen = true;
               continue;
            }   else if (ctok.comm == NO_COM) {
                    printf("Dont know what to do with %s\n", tok.str);
                    printf("May be there is an '-o' missing or look for 'EMPTY TOKEN' in the man pages\n");
                    exit(0);
            }

            strcat(l, tok.str); strcat(l, " ");

            if (seektok.type == END || seektok.comm != NO_COM ) {
                //printf("Adding line %s\n", l);
                removeTrailingSpaces(l);
                addline(&ctok, l);
                l[0] = '\0';
                ctok.comm  = NO_COM;
                continue;
            }


        }
    }
        

    //print script..
/*
    if (printscript || DEBUG) {
        struct line_t * li;
        prepBlobIteration(LineArray);
        while((li = getNextBlob(LineArray)) != NULL)  {
            printf("%i: %5s, %s\n", li->linenumber, gettokenname(li->com), li->str);
        }
    } 
*/
    if (!run) exit(0);

    //run script
    debprintf("--------------------\n");
    debprintf("Now run the script..\n");
    if (LineArray == NULL) debputs("There is none..");
    if (LineArray != NULL) { 
        //printf("Thats all for now... \n");
        //exit(0);
    struct line_t * cl;      //current line
    int linenumber = 1;
    cl = getLine(linenumber);
    do  {
        debprintf("Line %i :", linenumber);
        debprintf(" current command is : %i, with line %s length %li\n", cl->com , cl->str, strlen(cl->str));

        if (cl->com == OSC_COM) {
            debprintf("send osc message\n");
            sendOSCfromstr(cl->str);
        } else if (cl->com == PAUSE_COM) {
            usleep(atoi(cl->str) * 1000);
        } else if (cl->com == LOOP_COM) {
            linenumber = 0;
        } else if (cl->com == ECHO_COM) {
            printf("%s\n", cl->str);
        } else if (cl->com == OUT_COM) {
            if (strcmp(cl->str,"stdout") == 0) { OUT |= 0b100; OUT &= 0b110;}
            else if (strcmp(cl->str, "network")== 0) { OUT |= 0b001; OUT &=0b011;}
        } else if (cl->com == SCRIPT_COM) {
             struct line_t * li;
            prepBlobIteration(LineArray);
            while((li = getNextBlob(LineArray)) != NULL)  {
                printf("%i: %5s, %s\n", li->linenumber, /*gettokenname(li->com)*/ tokenlist[li->com].str, li->str);
            }
        }    
        linenumber++;
        cl = getLine(linenumber);
    }  while (cl != NULL);



    removeBlobArray(LineArray);
    
    }
    if (listen) pthread_join(th , NULL);

}

