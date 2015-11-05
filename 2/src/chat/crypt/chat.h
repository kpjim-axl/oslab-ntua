#ifndef CLIENT_H__
#define CLIENT_H__

#define PORT 50543
#ifndef MSG_SIZE
    #define MSG_SIZE 256
#endif
#define TCP_BACKLOG 5
#define MAX_CLIENTS 100

#include "io.h"

void client(int, char *, struct cryptodev_ctx *);
void server(int, struct cryptodev_ctx *);

#endif