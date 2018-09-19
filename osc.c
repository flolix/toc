#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "osc.h"

extern int DEBUG;


//int anzahlnulls (int i) {
//    return 4- (i & 0b11);
//}


int OSCString(char * buf, char * s) {
    //printf("in OSCstring with %s, len%i\n", s, strlen(s));
    strcpy(buf, s);
    int sl = strlen(s);
    int ol = (sl | 0b11)+1;
    sl++;
    //printf("ouput len: %i", ol);
    int i; 
    for (i = sl; i < ol; i++) buf[i]= '\0';
    return ol;
}

int OSCInt(char * buf, int32_t  v) {
    //turn the order of the int around
    int j = 0;
    int i;
    for (i = 3; i >= 0; i--) {
        buf[j] = *(((char *) &v)+i);
        j++;
    }
    return 4;
} 

int OSCFloat(char * buf, float v) {
    int j = 0;
    int i;
    for (i = 3; i >= 0; i--) {
        buf[j] = *(((char *) &v)+i);
        j++;
    }
    return 4;
}

struct OSCMsg_t *  OSCcreateMessagefromstr (char  * a) {
    char s[100];
    strcpy(s, a);
    char addr_buf[40];
    int addr_len;

    char typetable_buf[12];
    int typetable_len;

    char param_buf[40];
    int param[10];
    int param_len[10];

    int parcount = 0;
    char listbuf[12];
    char * space = strchr(s, ' ');
    int paramlength = 0;
    bzero(listbuf,12);
    listbuf[0] = ',';

    if (space == NULL) {
        //got no parameter only address
        addr_len = OSCString(addr_buf, s);
    } else {
        *space = '\0';
        addr_len = OSCString(addr_buf, s); 
        space ++;
        if (*space != '\0') { //to be sure it wasnt a trailing space
            char * space2;
            do {
                parcount++;
                param[parcount-1] = paramlength;         
                space2 = strchr(space, ' ');
                    if (space2 != NULL) *space2 = '\0';
                    //decide if its a string, int, float
                    if (*space == '"') {
                        if (DEBUG) printf("Oh, its a string (with quotation marks)!\n");
                        if (space2 != NULL) *(space2-1) = '\0'; //eliminate the trailing "
                        else space[strlen(space)-1] = '\0';
                        param_len[parcount-1] = OSCString(param_buf+paramlength, space+1); //+1 because of leading "
                        paramlength += param_len[parcount-1];
                        listbuf[parcount] = 's';  
                    } else {
                        char * endptr;
                        long int val = strtol(space, &endptr, 0); 
                        if (*endptr == '\0') {
                            if (DEBUG) printf("Oh, its an int! \n");
                            param_len[parcount-1] = OSCInt(param_buf+paramlength, (int32_t) val);
                            paramlength += param_len[parcount-1];
                            listbuf[parcount] = 'i';  
                        } else {
                            float val = strtof(space, &endptr);
                            if (*endptr == '\0') {
                                if (DEBUG) printf("Oh, its a float! \n");
                                param_len[parcount-1] = OSCFloat(param_buf+paramlength, (float) val);
                                paramlength += param_len[parcount-1];
                                listbuf[parcount] = 'f';  
                            } else {
                                //must be a string.. nothing else left
                                if (DEBUG) printf("Oh, its a string!\n");
                                param_len[parcount-1] = OSCString(param_buf+paramlength, space);
                                //param[parcount-1] = param_buf+ paramlength;
                                paramlength += param_len[parcount-1];
                                listbuf[parcount] = 's';  
                            }
                        }
                    }
                    space = space2+1;
                } while(space2 != NULL);
            }
    }

    typetable_len = OSCString(typetable_buf, listbuf);
    //move oscmsg;
    struct OSCMsg_t  * r;
    r = malloc(sizeof(struct OSCMsg_t));
    r->len = addr_len + typetable_len + paramlength;
    r->buf = malloc(r->len);
    memcpy(r->buf, addr_buf, addr_len);
    r->addr_len = addr_len;
    r->typetable = r->buf + addr_len;
    memcpy(r->typetable, typetable_buf, typetable_len);
    r->typetable_len = typetable_len;
    if (parcount > 0) {
        memcpy(r->buf + addr_len + typetable_len, param_buf, paramlength);
    }
    r->addr = r->buf;
    r->paramcount = parcount;
    int i;
    for (i = 0 ; i< parcount; i++) {
        r->param[i] = r->buf + addr_len + typetable_len + param[i];
        r->param_len[i] = param_len[i];
    } 
    return r;
}

/*
char * sncreateOSCMessage (char * buf, int * length, char * command, char * paramlist, ...) {
    va_list argumente;
    int anzahl, commandlength, i, an;
    int32_t intarg;
    va_start (argumente, paramlist);
    anzahl = strlen(paramlist);
    
    commandlength = strlen(command);
    strcpy(buf, command);

    an = anzahlnulls(commandlength);
    for (i = 0; i<an; i++) buf[commandlength+i] = '\0';
    commandlength += an;
    buf[commandlength] = ',';
    commandlength++;
    strcpy(&buf[commandlength], paramlist);
    commandlength += anzahl;
    an = anzahlnulls(anzahl+1);
    for (i = 0; i< an; i++) buf[commandlength +i] = '\0';
    commandlength += an;
    int j;
    for (i = 0; i< anzahl; i++) {
        if (paramlist[i] == 'i') {
            intarg = va_arg(argumente, int32_t);
            char * test = (char *) &intarg;
            for (j = 3; j>=0; j--) {
                buf[commandlength] = * (test+j);
                commandlength++;
            }
        }
    }
    va_end (argumente);
    *length = commandlength;

}
*/
enum {OSC_DONE, OSC_ADDRESS, OSC_TYPETABLE, OSC_INTEGER, OSC_STRING, OSC_FLOAT, OSC_CHECKFORNEXTPARAM};


void OSCChangeorder(void * mem) {
    char buf[4];
    buf[0] = *((char*) mem+3);
    buf[1] = *((char*) mem+2);
    buf[2] = *((char*) mem+1);
    buf[3] = *((char*) mem+0);
    memcpy(mem, buf, 4);
}

int32_t OSCtoInt(char * buf) {
    int32_t v;
    *(((char *)&v)+0) = *(buf+3);
    *(((char *)&v)+1) = *(buf+2);
    *(((char *)&v)+2) = *(buf+1);
    *(((char *)&v)+3) = *(buf+0);
    return v;
}

float OSCtoFloat(char * buf) {
    float f;
    *(((char *)&f)+0) = *(buf+3);
    *(((char *)&f)+1) = *(buf+2);
    *(((char *)&f)+2) = *(buf+1);
    *(((char *)&f)+3) = *(buf+0);
    return f;
}

struct OSCMsg_t * OSCsplit(char * buf, int len) {
    struct OSCMsg_t * o;
    o = malloc(sizeof(struct OSCMsg_t));
    o->buf = malloc(len);
    memcpy(o->buf, buf, len);
    o->paramcount = 0;
    int i;
    int lasti;
    int state = OSC_ADDRESS;
    for (i = 3; i< len; i+=4) {
        if (state == OSC_ADDRESS && o->buf[i] == '\0') {
            o->addr = buf;
            o->addr_len = i+1;
            state = OSC_TYPETABLE;
            lasti = i+1;
        } else if (state == OSC_TYPETABLE && o->buf[i] == '\0') {
            o->typetable = o->buf+lasti;
            o->typetable_len = i- lasti+1;
            state = OSC_CHECKFORNEXTPARAM;
        } else if (state == OSC_INTEGER) {
            o->param[o->paramcount-1] = o->buf+lasti;
            //OSCChangeorder(o.param[o.paramcount-1]);
            o->param_len[o->paramcount-1] = 4;
            state = OSC_CHECKFORNEXTPARAM;
        } else if (state == OSC_STRING && o->buf[i] == '\0') {
            o->param[o->paramcount-1] = o->buf+lasti;
            o->param_len[o->paramcount-1] = i - lasti + 1;
            state = OSC_CHECKFORNEXTPARAM; 
        } else if (state == OSC_FLOAT) {
            o->param[o->paramcount-1] = o->buf+lasti;
            o->param_len[o->paramcount-1] = 4;
            state = OSC_CHECKFORNEXTPARAM;
        }
        
        if (state == OSC_CHECKFORNEXTPARAM) {
            if (o->paramcount > MAXPARAM - 1) state = OSC_DONE;
            else { 
                
            if (o->typetable[o->paramcount+1] == '\0') state = OSC_DONE;
            else {
                lasti = i+1;
                o->paramcount++;
                if (o->typetable[o->paramcount] == 'i') state = OSC_INTEGER;
                if (o->typetable[o->paramcount] == 's') state = OSC_STRING;
                if (o->typetable[o->paramcount] == 'f') state = OSC_FLOAT;
            }
            }
        }

    }
    return o;
}

void OSCtoStr (char * buf, struct OSCMsg_t * osc) {
    buf[0] = '\0';
    char buff[100];
    int i;
    sprintf(buf, "%s ", osc->addr);
    for (i = 0; i < osc->paramcount; i++) {
        if (osc->typetable[i+1] == 'i') sprintf(buff, "%i ", OSCtoInt(osc->param[i]));
        if (osc->typetable[i+1] == 'f') sprintf(buff, "%f ", OSCtoFloat(osc->param[i]));
        if (osc->typetable[i+1] == 's') sprintf(buff, "%s ", osc->param[i]);
        //printf("%s", buff);
        strcat(buf, buff);
        //printf("buf %s", buf);
    }
    sprintf(buff, "( %i parameter)", osc->paramcount);
    strcat (buf, buff);
}

void OSCPrintMsg (struct OSCMsg_t * osc) {
    /*
    int i,j;
    for (i = 0; i<osc.addr_len; i++) printf("%x ", osc.addr[i]);
    for (i = 0; i<osc.typetable_len; i++) printf("%x ", osc.typetable[i]);
    for (i = 0; i< osc.paramcount; i++) for (j = 0; j<osc.param_len[i]; j++) printf("%x ", osc.param[i][j]);
    printf("\n");
    */
    char buf[100];
    OSCtoStr (buf, osc);
    puts(buf);
/*
    printf("%s, %i paramter:", osc.addr, osc.paramcount);
    for (i = 0; i < osc.paramcount; i++) {
        if (osc.typetable[i+1] == 'i') printf("%i ", OSCtoInt(osc.param[i]));
        if (osc.typetable[i+1] == 'f') printf("%f ", OSCtoFloat(osc.param[i]));
        if (osc.typetable[i+1] == 's') printf("%s ", osc.param[i]);
    }
    printf("\n");
*/
    fflush(stdout);
}

void OSCFreeMessage(struct OSCMsg_t * osc) {
    free(osc->buf);
    free(osc);
}

bool OSCcheckaddress(char * addr) {
    bool valid = false;
    if (addr[0] == '/') valid = true;
    if (!valid) {
        printf("OSC address '%s' is not valid! I wont send it. Try harder next time..\n", addr);
        exit(EXIT_FAILURE);
    }
    return true;
}

