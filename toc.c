#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "help.h"
#include "osc.h"
#include "color.h"
#include "blobarray.h"


int udpsocket = -1;
struct sockaddr_in si_udp;
char IP[20] = "";
int PORT;

void init_socket(){
    if ((udpsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        puts("could not create socket");
    }
    memset((char *) &si_udp, 0, sizeof(si_udp));
    si_udp.sin_family = AF_INET;
    si_udp.sin_port = htons(PORT);
    if (inet_aton(IP, &si_udp.sin_addr) == 0) {
        printf("aton failed with ip %s\n", IP);
        close(udpsocket);
        udpsocket = 55;
        return;
    }
}

void sendUDP (char * buf, int len) {
    if (udpsocket == -1) init_socket();
    if (sendto (udpsocket, buf, len, 0, (struct sockaddr *) &si_udp, sizeof(struct sockaddr_in)) == -1) {
        puts("could not sent");
        perror("sendUDP");
        printf("socket: %i, buf:%s, len:%i, ip:%s, port:%i\n", udpsocket, buf, len, inet_ntoa(si_udp.sin_addr), si_udp.sin_port);
    }
}

int DEBUG = 0;

/*
void sendOSC(struct command_t c) {
    if (udpsocket == -1) init_socket();
    char buf[100];
    int len;
    if (DEBUG) {
        int i;
        setcolor(GREEN);
        printf("Send %s ",c.comm);
        for (i = 0; i < c.pc; i++) printf("%s ", c.param[i]);
        printf("(%i) to %s\n", c.pc, IP);    
        setcolor(NOCOLOR); 
    }
    createOSCMessage(buf, &len, c);
    sendUDP(buf, len);
}
*/
int OUT = 0b001;

void sendOSCfromstr(char * s) {
    int len;
    char buf[100];
    //make a copy before hand it to createOSCMessage.. better safe then..
    char copy[100];
    strcpy(copy, s);
    createOSCMessagefromstr(buf, &len, copy);
    //OUT:
    // Bit 3: print to stdout, Bit 2: print in hex to stdout, Bit 1: send via udp
    if ((OUT & 1) == 1) sendUDP(buf, len);
    if ((OUT & 2) == 2) {
        int i;
        for (i = 0; i< len; i++) {
            printf("%.2x ", (uint8_t) buf[i]);
        }
        puts("");
    }
    if ((OUT & 4) == 4) {
        int i;
        for (i = 0; i< len; i++) {
            putc(buf[i], stdout);
        }
    }
}

struct token_t {
    char * chars;
    int len;
    int type;
    char str[40];
    int comm;
};

enum { NON , START, END};

enum {NO_COM, ALIAS_COM, PAUSE_COM, OSC_COM, LOOP_COM, ECHO_COM, DEBUG_COM , IP_COM, PORT_COM, OUT_COM};
struct tokenlist_t {
    int com;
    char str[10];
} tokenlist[15] = {{NO_COM, ""}, {ALIAS_COM, "-a"}, {PAUSE_COM, "-p"}, {OSC_COM, "-o"}, {LOOP_COM, "-l"}, {ECHO_COM, "-e"}, {DEBUG_COM, "-d"}, {IP_COM, "-ip"},{PORT_COM,"-port"},{OUT_COM,"-out"},{99}};


char * gettokenname(int tcom) {
    int i = 0;
    while (tokenlist[i].com != 99 && tokenlist[i].com != tcom) i++;
    if (tokenlist[i].com == 99) return ""; else return tokenlist[i].str;
}

struct token_t getnexttoken(struct token_t tok, char * line) {
    struct token_t t;
    t.type = NON;
    t.len = 0;
    t.str[0] = '\0';
    t.chars = NULL;
    t.comm = NO_COM;
    if (tok.type == START) {
        //first invoke
        t.chars = line;
    } else {
        if (tok.chars - line < strlen(line)) {
            t.chars = strchr(tok.chars, ' ');
            if (t.chars != NULL) t.chars++;
        }
        else t.chars = NULL;
    }

    if (t.chars == NULL || *t.chars == '\0' ) {
        t.type = END;
        return t;
    }
    char * nextspace = strchr(t.chars, ' ');
    if (nextspace == NULL) t.len = strlen(t.chars);
    else t.len =  nextspace - t.chars; 
    strncpy(t.str, t.chars, t.len);
    t.str[t.len] = '\0';

    if (strcmp(t.str, "-o") == 0) t.comm = OSC_COM;
    else if (strcmp(t.str, "-a") == 0) t.comm = ALIAS_COM;
    else if (strcmp(t.str, "-p") == 0) t.comm = PAUSE_COM;
    else if (strcmp(t.str, "-l") == 0) t.comm = LOOP_COM;
    else if (strcmp(t.str, "-e") == 0) t.comm = ECHO_COM;
    else if (strcmp(t.str, "-d") == 0) t.comm = DEBUG_COM;
    else if (strcmp(t.str, "-ip") == 0) t.comm = IP_COM;
    else if (strcmp(t.str, "-port") == 0) t.comm = PORT_COM;
    else if (strcmp(t.str, "-out") == 0) t.comm = OUT_COM;
    else t.comm = NO_COM;
    return t;
} 
  
void printconfigfile(char * name) {
    FILE * datei;
    char line[100];
    datei = fopen(name, "r");  
    if (datei == NULL) {
        perror("fopen");
        exit(1);
    }   
    int i = 1;
    printf("Config file is %s\n", name);
    //puts  ("");
    while( fgets(line,99,datei)) {
        //remove trailing newline if any..
        if (line[strlen(line)-1] == '\n') line[strlen(line)-1] = '\0';
        printf("%s\n", line);
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

void applyAliase (char * line) {
    struct alias_t * a;
    char newline[100];
    newline[0] = '\0';
    char * st = line;

    prepBlobIteration(AliasArray);
    while ((a = getNextBlob(AliasArray)) != NULL)  {
        char * s;
        while ((s = strstr(st, a->s)) != NULL) {
            strncat(newline, st, s - st);
            strcat(newline, a->r);
            st = s + strlen(a->s);
        }
    } 
    strcat(newline, st);
    strcpy(line, newline);
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
       

void checkaddressvalidity(char * line) {
    bool valid = false;
    char address[40];
    //get address .. skip '-o' command
    char * space;
    space = strchr(line, ' '); 
    if (space != NULL)  {
        strncpy(address, line, space - line);
        address[space-line] = '\0';
    }
    else strcpy(address, line);

    if (address[0] == '/') valid = true;

    if (!valid) {
        //setcolor(RED);
        printf("OSC address '%s' not valid! I wont send it. Try harder..\n", address);
        exit(EXIT_FAILURE);
    }
}

int main (int argc, char ** argv) {
    //find the path of the config file by
    // using the system which tool
    FILE *pf;
    char configfile[100];
    pf = popen ("which toc", "r");
    fgets (configfile, 99, pf);
    pclose(pf);
    //ok got the path.. now removing the last \n    
    configfile[strlen(configfile)-1] = '\0';
    //and just adding .config
    strcat(configfile, ".config");
    if (DEBUG) printf("Config file is: %s\n", configfile); 

    //evaluating commandline arguments.. there are some special commands like
    //? and config and script..
    //concatenate the rest to one chunk
    bool printscript = false;
    bool run = true;
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
            printf("DEBUG messages activated\n");
            DEBUG = 1;
        } else if (strcmp(argv[i], "script") == 0) {
            printscript = true;
            run = false;
            continue;
        }
        strcat(commandline, argv[i]);
        strcat(commandline, " ");
    };

  
    //opening config file 
    FILE * datei;
    datei = fopen(configfile, "r");
    fseek(datei,0,SEEK_END);
    long int filesize = ftell(datei);  // thats the file size
    fseek(datei,0,SEEK_SET);

    //now parse everything
    char line [100];
    bool done = false;
    while (!done) {
        if (fgets(line, 99, datei)) { // got a line from config file
            //at first we remove the newline char..
            if (line[strlen(line)-1] == '\n') line[strlen(line)-1] = '\0';
            if (DEBUG) printf("Got line from file >%s<\n", line); 
            applyAliase(line);
            char * ap;
            if ((ap = strstr(line, " -a ")) != NULL) {
                //alias definition found
                *(ap) = '\0';
                defineAlias(line, ap+4);
                continue;
            }
        } else {
            strcpy(line, commandline);
            if (DEBUG) printf("Got commandline arguments >%s<\n", line);
            applyAliase(line);
            done = true; // this is the last line..
        }
       
        fflush(stdout);

        struct token_t tok;
        struct token_t ctok;
        tok.type = START;
        int current_comm = NO_COM;
        char l[50] ="";
       
        while (tok = getnexttoken(tok, line), tok.type != END) {
            if (DEBUG) printf("current token is : %s, with length %d\n", tok.str , tok.len);
            struct token_t seektok = getnexttoken(tok, line);
            if (DEBUG) printf("and seek token is : %s, with length %d, %d\n", seektok.str , seektok.len, seektok.comm);
            fflush(stdout);

            if (tok.comm == DEBUG_COM) {
                DEBUG = 1;
                current_comm = NO_COM;
                continue;
            } else if (tok.comm == OSC_COM) {
                current_comm = OSC_COM;
                ctok = tok;
                l[0] = '\0';
                continue;
            } else if (tok.comm == PAUSE_COM) {
                ctok = tok;
                tok = getnexttoken(tok, line);
                current_comm = NO_COM;
                addline(&ctok, tok.str);
                continue;
            } else if (tok.comm == IP_COM) {
                ctok = tok;
                tok = getnexttoken(tok, line);
                current_comm = NO_COM;
                addline(&ctok, tok.str);
                continue; 
            } else if (tok.comm == PORT_COM) {
                ctok = tok;
                tok = getnexttoken(tok, line);
                current_comm = NO_COM;
                addline(&ctok, tok.str);
                continue; 
            } else if (tok.comm == OUT_COM) {
                ctok = tok;
                tok = getnexttoken(tok, line);
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
            }
        
            if (current_comm == NO_COM && tok.comm == NO_COM) {
                if (DEBUG) printf("type ist currently undefined! considering it is an osc address\n");
                current_comm = OSC_COM;
                ctok.comm = OSC_COM;
            }
            strcat(l, tok.str);
            if (seektok.type == END) {
                if (ctok.comm == OSC_COM) checkaddressvalidity(l);
                addline(&ctok, l);
            } else
                if (seektok.comm != NO_COM) {
                    if (ctok.comm == OSC_COM) checkaddressvalidity(l);
                    addline(&ctok, l); 
                } else strcat(l, " ");
        }

    }
        
    fclose(datei); 

    //print script..
    if (printscript || DEBUG) {
        struct line_t * li = getFirstBlob(LineArray);
        do {
            printf("%i: %5s, %s\n", li->linenumber, gettokenname(li->com), li->str);
        } while((li = getNextBlob(LineArray)) != NULL) ;
    } 

    if (!run) exit(0);

    //run script
    if (DEBUG) {
        printf("--------------------\n");
        printf("Now run the script..\n");
    }
    if (LineArray == NULL) { // no script there
        printf("Thats all for now... \n");
        exit(0);
    }
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
            printf("%s\n ", cl->str);
        } else if (cl->com == IP_COM) {
            strcpy(IP, cl->str);
            if (DEBUG) printf("Setting IP: %s\n", IP);
        } else if (cl->com == PORT_COM) {
            PORT = atoi(cl->str);
            if (DEBUG) printf("Setting PORT: %i\n", PORT);
        } else if (cl->com == OUT_COM) {
            if (strcmp(cl->str,"stdout") == 0) { OUT |= 0b100; OUT &= 0b110;}
            else if (strcmp(cl->str, "network")== 0) { OUT |= 0b001; OUT &=0b011;}
        }
        linenumber++;
        cl = getLine(linenumber);
    }  while (cl != NULL);


    removeBlobArray(LineArray);

}

