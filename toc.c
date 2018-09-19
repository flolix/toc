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



int RECEIVE_COLOR = GREEN;
int SEND_COLOR = NOCOLOR;


char IP[20] = "<no ip>";
int PORT = 0;


int DEBUG = 0;
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
    setcolor(SEND_COLOR);
    printf("<-- %s\n",s);
    setcolor(NOCOLOR);
    struct OSCMsg_t * oscmsg;
    oscmsg = OSCcreateMessagefromstr(s);
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
} tokenlist[15] = {{NO_COM, ""}, {PAUSE_COM, "-p"}, {OSC_COM, "-o"}, {LOOP_COM, "-l"}, {ECHO_COM, "-e"}, {OUT_COM,"-out"},{SCRIPT_COM, "-script"},{LISTEN_COM, "listen"},{99}};


char * gettokenname(int tcom) {
    int i = 0;
    while (tokenlist[i].com != 99 && tokenlist[i].com != tcom) i++;
    if (tokenlist[i].com == 99) return ""; else return tokenlist[i].str;
}

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
    char line[100];
    datei = fopen(name, "r");  
    if (datei == NULL) {
        perror("open config file failed");
        exit(1);
    }   
    int i = 1;
    printf("Config file is %s\n", name);
    //puts  ("");
    while( fgets(line,99,datei)) {
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

void defineAlias(char * search, char * replace) {
    if (DEBUG) printf("New alias definition: search for %s -> replace by %s\n", search, replace);
    struct alias_t * newalias = malloc(sizeof(struct alias_t));
    newalias->s = malloc(strlen(search)+1);
    strcpy(newalias->s, search);
    newalias->r = malloc(strlen(replace)+1);
    strcpy(newalias->r, replace);
    appendBlob(&AliasArray, newalias);
}

void applyAlias(struct alias_t * a, char * result, char * work) {
    result[0] = '\0';
    char * s;
    char * resptr = result;
    while ((s = strstr(work, a->s)) != NULL) {
        if (DEBUG) printf("ALIAS %s found!\n", a->s);
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

void applyAliasAtStart(struct alias_t * a, char * result, char * work) {
    result[0] = '\0';
    //char * save = work;
    if (strncmp(work, a->s, strlen(a->s)) == 0) {
        if (DEBUG) printf("ALIASAtStart %s found!\n", a->s);
        //printf("applyAlias: Firstpart is: %i\n", s-work);
        strcpy(result, a->r);
        work += strlen(a->s);
    }
    strcat(result, work);
    //work = save;
}

void  applyAliaseAtStart (char * result, char * line) {
    //printf("applyAliaseAtStart: Working on line %s\n", line);
    struct alias_t * a;
    char between[1000];
    strcpy(between, line);
    prepBlobIteration(AliasArray);
    while ((a = getNextBlob(AliasArray)) != NULL)  {
        applyAliasAtStart(a, result, between);
        strcpy(between, result); 
        //printf("applyAliase: Between %s\n", between);
    } 
    strcpy(result, between);
    //printf("applyAliase: result is %s\n", result);
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
    puts("Alias table");
    prepBlobIteration(AliasArray);
    i = 1;
    struct alias_t * a;
    while ((a = getNextBlob(AliasArray)) != NULL) {
        printf("%i: %s -> %s\n",i, a->s, a->r);
        i++;
    }
}

int main (int argc, char ** argv) {
    char configfile[100] = "";
    makingconfigpath(configfile);
    //evaluating commandline arguments.. there are some special commands like
    //? and config and script..
    //concatenate the rest to one chunk
    bool run = true;
    bool listen = false;
    char commandline [200] = "";
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
            setcolor(RED);
            printf("DEBUG messages activated\n");
            setcolor(NOCOLOR);
            DEBUG = 1;
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
        }

        strcat(commandline, argv[i]);
        strcat(commandline, " ");
    };

    FILE * datei;
    char DEF_COM[10] = "";
    datei = fopen(configfile, "r");
    if (datei == NULL) 
        puts("The config file does not exist. Please refer to 'toc -?' if you dont know what I am writing about..");
    else {
        //raed it..
        char line[200];
        while (fgets(line, 200, datei)) {
            if (line[0] == '/' && line[1] == '/') continue;
            if (line[0] == '#') continue;
            if (line[0] == '\n') continue;
            char result[1000];
            //newline entfernen..
            if (line[strlen(line)-1] == '\n') line[strlen(line)-1] = '\0';
            applyAliase(result, line);
            char * ap;
            if ((ap = strstr(result, " := ")) != NULL) {
                *ap = '\0';
                defineAlias(result, ap+4);
                *ap = ' ';
                continue;
            } 
            if (strncmp(result, ":= ",3) == 0) {
                strcpy(DEF_COM, result+3);
                if (DEBUG) printf("Default Token is set to %s\n", DEF_COM);
                continue;
            }

if (strlen(DEF_COM) > 0) {       
    struct token_t tok;
    int c = 0;
    char * ptr = result;
    while(tok = getnexttoken(tok, ptr), tok.type != END) {
        ptr = NULL;
        if (tok.comm != NO_COM) c = tok.comm;
        if (tok.comm == NO_COM && c == NO_COM) {
            if (DEBUG) printf("undefined token..%s\n", tok.str);
            //memmove(result+3, result, strlen(result)+1);
            memmove(result + strlen(DEF_COM) + 1, result, strlen(result) + 1);
            strcpy(result, DEF_COM);
            result[strlen(DEF_COM)] = ' ';
            //strncpy(result, "-o  ", 3);
            char result2[200];
            applyAliaseAtStart(result2, result);
            strcpy(result, result2);
            result2[0] = '\0';
            ptr = result2;
        }
    }
}    

            addconfigline(result);
        } 
        if (DEBUG) puts("Config file has been processed.");
        fclose(datei);
    }


printAliasTable();

char result[200];
applyAliase(result, commandline);
    //now replace missing command at commandline
if (strlen(DEF_COM) > 0) {       
    struct token_t tok;
    int c = 0;
    char * ptr = result;
    while(tok = getnexttoken(tok, ptr), tok.type != END) {
        ptr = NULL;
        if (tok.comm != NO_COM) c = tok.comm;
        if (tok.comm == NO_COM && c == NO_COM) {
            if (DEBUG) printf("undefined token..%s\n", tok.str);
            memmove(result + strlen(DEF_COM) + 1, result, strlen(result) + 1);
            //strncpy(result, "-o  ", 3);
            strcpy(result, DEF_COM);
            result[strlen(DEF_COM)] = ' ';
            char result2[200];
            applyAliaseAtStart(result2, result);
            strcpy(result, result2);
            result2[0] = '\0';
            ptr = result2;
        }
    }
}
    addconfigline(result);



    //now parse everything
    char * currentline = NULL;
    prepBlobIteration(ConfigFile);
    while ((currentline = getconfigline()) != NULL) {
        struct token_t tok;
        struct token_t ctok;
        ctok.comm = NO_COM;
        tok.type = START;
        int current_comm = NO_COM;
        char l[50] ="";
       
        while (tok = getnexttoken(tok, currentline), tok.type != END) {
            currentline = NULL;
            if (DEBUG) printf("current token is : %s, with length %d, and com %d\n", tok.str , tok.len, tok.comm);
            struct token_t seektok = getnexttoken(tok, NULL);
            if (DEBUG) printf("and seek token is : %s, with length %d, and type %d\n", seektok.str , seektok.len, seektok.type);
            fflush(stdout);

            if (tok.type == EOL) continue; // an empty line
                
            if (tok.comm == OSC_COM) {
                current_comm = OSC_COM;
                ctok = tok;
                l[0] = '\0';
                continue;
            } else if (tok.comm == PAUSE_COM) {
                ctok = tok;
                tok = getnexttoken(tok, NULL);
                current_comm = NO_COM;
                addline(&ctok, tok.str);
                continue;
           } else if (tok.comm == OUT_COM) {
                ctok = tok;
                tok = getnexttoken(tok, NULL);
                current_comm = NO_COM;
                addline(&ctok, tok.str); 
                continue;
            } else if (tok.comm == LOOP_COM) {
                current_comm = NO_COM;
                addline(&tok, "");
                continue;
            } else if (tok.comm == ECHO_COM) {
                ctok = tok;
                l[0] = '\0';
                current_comm = ECHO_COM;
                continue;
            } else if (tok.comm == SCRIPT_COM) {
                current_comm = NO_COM;
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
            }

           strcat(l, tok.str);
            if (seektok.type == END || seektok.comm != NO_COM) {
                if (ctok.comm == OSC_COM) {
                    //checkOSCcommand(l);
                    //struct OSCMsg_t * oscmsg = OSCcreateMessagefromstr(l);
                    //OSCcheckaddress(oscmsg->addr);
                    //OSCFreeMessage(oscmsg);
                } else if (ctok.comm == NO_COM) {
                    //printf("Addline with ctok %i \n", ctok.comm);
                    printf("Dont know what to do with %s\n", l);
                    printf("May be there is an '-o' missing or look for 'DEFAULT TOKEN' in the man pages\n");
                    exit(0);
                }
                addline(&ctok, l);
                l[0] = '\0';
            } else strcat(l, " ");
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
    if (DEBUG) {
        printf("--------------------\n");
        printf("Now run the script..\n");
        if (LineArray == NULL) puts("There is none..");
    }
    if (LineArray != NULL) { // no script there
        //printf("Thats all for now... \n");
        //exit(0);
    struct line_t * cl;      //current line
    int linenumber = 1;
    cl = getLine(linenumber);
    do  {
        if (DEBUG) {
            printf("Line %i :", linenumber);
            printf(" current command is : %i, with line %s length %li\n", cl->com , cl->str, strlen(cl->str));
            fflush(stdout);
        }

        if (cl->com == OSC_COM) {
            if (DEBUG) printf("send osc message\n");
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
                printf("%i: %5s, %s\n", li->linenumber, gettokenname(li->com), li->str);
            }
        }    
        linenumber++;
        cl = getLine(linenumber);
    }  while (cl != NULL);



    removeBlobArray(LineArray);
    
    }
    if (listen) pthread_join(th , NULL);

}

