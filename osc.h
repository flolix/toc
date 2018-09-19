struct OSCMsg_t {
    char * buf;
    int len;
    char * addr;
    int addr_len;
    char * typetable;
    int typetable_len;

    char * param[10];
    int param_len[10];
    int paramcount;
};
#define MAXPARAM 10
 
struct OSCMsg_t * OSCcreateMessagefromstr (char * s);
struct OSCMsg_t * OSCsplit(char * buf, int len);
void OSCPrintMsg (struct OSCMsg_t * osc);
void OSCtoStr (char * buf, struct OSCMsg_t * osc);
void OSCFreeMessage(struct OSCMsg_t * osc);
bool OSCcheckaddress(char * addr);
