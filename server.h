#include "tftp.h"
#include <pthread.h>
struct tftpreq {
    struct sockaddr_in client;
    char packet[PACKET_BUF_SIZE];
    int  len;
};
