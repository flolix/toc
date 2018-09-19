
struct fredparams_t {
    char ip[20];
    int port;
}; 


void * listenfred (void  * arg);
void sendUDP (char * buf, int len, char * ip, int port);
