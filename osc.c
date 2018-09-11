#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern int DEBUG;


int anzahlnulls (int i) {
    return 4- (i & 0b11);
}

/*
void createOSCMessage (char * buf, int * length, struct command_t c) {
    int32_t intparam;
    int an, commandlength, i;
    commandlength = strlen(c.comm);
    strcpy(buf,c.comm);
    an = anzahlnulls(commandlength);
    for (i = 0; i< an; i++) { buf[commandlength] = '\0'; commandlength ++;}
    buf[commandlength] = ',';
    commandlength++;
    for (i = 0; i < c.pc; i++) {buf[commandlength] = 'i'; commandlength++;}
    an = anzahlnulls(c.pc + 1);
    for(i = 0; i<an; i++) {buf[commandlength] = '\0';commandlength++;}
    int j;
    for (i = 0; i < c.pc;i++) {
        //suggest parameter is int32_t
        intparam = atoi(c.param[i]);
        char * test = (char *) &intparam;
        for (j = 3;j>=0; j--) {
            buf[commandlength] = *(test+j);
            commandlength++;
        }
    }
    *length = commandlength;
}
*/


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

void createOSCMessagefromstr (char * buf, int * length, char s[]) {
    int msglength;
    int parcount = 0;
    char parambuf[100];
    char listbuf[12];
    bzero(listbuf, 99);
        char * space = strchr(s, ' ');
        if (space == NULL) {
            msglength = OSCString(buf, s);
            msglength += OSCString(buf+msglength, ",");
        } else {
            *space = '\0';
            msglength = OSCString(buf, s); 
            listbuf[0] = ',';
            space ++;
            if (*space != '\0') {
                int paramlength = 0;
                char * space2;
                do {
                    parcount++;
                    space2 = strchr(space, ' ');
                    if (space2 != NULL) *space2 = '\0';
                    //decide if its a string, int, float
                    if (*space == '"') {
                        if (DEBUG) printf("Oh, its a string (with quotation marks)!\n");
                        if (space2 != NULL) *(space2-1) = '\0'; //eliminate the trailing "
                        else space[strlen(space)-1] = '\0';
                        paramlength += OSCString(parambuf+paramlength, space+1); //+1 because of leading "
                        listbuf[parcount] = 's';  
                    } else {
                        char * endptr;
                        long int val = strtol(space, &endptr, 0); 
                        if (*endptr == '\0') {
                            if (DEBUG) printf("Oh, its an int! \n");
                            paramlength += OSCInt(parambuf+paramlength, (int32_t) val);
                            listbuf[parcount] = 'i';  
                        } else {
                            float val = strtof(space, &endptr);
                            if (*endptr == '\0') {
                                if (DEBUG) printf("Oh, its a float! \n");
                                paramlength += OSCFloat(parambuf+paramlength, (float) val);
                                listbuf[parcount] = 'f';  
                            } else {
                                //must be a string.. nothing else left
                                if (DEBUG) printf("Oh, its a string!\n");
                                paramlength += OSCString(parambuf+paramlength, space);
                                listbuf[parcount] = 's';  
                            }
                        }
                    }
                            
                    space = space2+1;
                } while(space2 != NULL);
                //put togehter..
                msglength += OSCString(buf + msglength, listbuf);
                memcpy(buf+msglength, parambuf, paramlength);
                msglength += paramlength;
            }
        }
    *length = msglength;
}


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
