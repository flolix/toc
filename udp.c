#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "color.h"
#include "osc.h"

extern int RECEIVE_COLOR;
extern int SEND_COLOR;

bool isMulticastAddress (struct in_addr * addr) {
    //reverse order from network to system order
    short highest = ntohl(addr->s_addr) >> 24;
    if (( highest & 0b11110000) == 0b11100000 ) return true;
    else return false;
}


int init_udp_recv(char * ip, int port) {
    struct sockaddr_in addr;
    int fd;
    struct ip_mreq mreq;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    int loop = 1;
    if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &loop, sizeof(loop)) < 0) {
        perror("setsockopt: SO_REUSADDR");
        exit(EXIT_FAILURE);
    }
    if (bind(fd, (struct sockaddr * ) &addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }
    
    struct in_addr inp;
    if (inet_aton(ip, &inp) == 0) {
        perror("inet_aton failed");
        printf("%s\n", ip);
        exit(1);
    }
    
    if (isMulticastAddress(&inp)) {
        setcolor(RECEIVE_COLOR);
        printf("Listening to multicast address %s, port %i\n", inet_ntoa(inp), port);
        setcolor(NOCOLOR);
        mreq.imr_multiaddr = inp;
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            perror("setsockopt");
            exit(1);
        }
    } else {
        setcolor(RECEIVE_COLOR);
        printf("Listening to address %s, port %i\n", inet_ntoa(inp), port);
        setcolor(NOCOLOR);
    }
    return fd;
}


struct fredparams_t {
    char ip[20];
    int port;
}; 

void * listenfred (void  * arg) {
    //int slen;
    int retval;
    fd_set recvset;

    char buf[200];
    char buff[200];
    #define BUFLEN 200
    struct fredparams_t * params =  (struct fredparams_t *) arg;
    int fd = init_udp_recv(params->ip, params->port);
    while (1) {
        FD_ZERO(&recvset);
        FD_SET(fd, &recvset);
        retval = select(fd + 1 , &recvset, NULL, NULL, NULL);
        if (retval > 0) {
            if (FD_ISSET(fd, &recvset)) {
                //int recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr*) &addr, &slen);
                int recvlen = recv(fd, buf, BUFLEN, 0);
                struct OSCMsg_t * oscmsg;
                oscmsg = OSCsplit(buf, recvlen);
                OSCtoStr(buff, oscmsg);
                setcolor(GREEN);
                printf("--> %s\n",buff);
                setcolor(NOCOLOR);
                OSCFreeMessage(oscmsg);
            }
        }
    } 
    return NULL;
}

struct sockaddr_in si_udp;

int init_socket(char * ip, int port){
    int udpsocket = -1;
    if ((udpsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        puts("could not create socket");
    }
    memset((char *) &si_udp, 0, sizeof(si_udp));
    si_udp.sin_family = AF_INET;
    si_udp.sin_port = htons(port);
    if (inet_aton(ip, &si_udp.sin_addr) == 0) {
        printf("aton failed with ip %s\n", ip);
        close(udpsocket);
        return -1;
    }
    return udpsocket;
}

void sendUDP (char * buf, int len, char * ip, int port) {
    static int sendsock = -1;;
    if (sendsock == -1) sendsock = init_socket(ip, port);
    if (sendto (sendsock, buf, len, 0, (struct sockaddr *) &si_udp, sizeof(struct sockaddr_in)) == -1) {
        perror("sendUDP");
        printf("socket: %i, buf:%s, len:%i, ip:%s, port:%i\n", sendsock, buf, len, inet_ntoa(si_udp.sin_addr), si_udp.sin_port);
    }
}

