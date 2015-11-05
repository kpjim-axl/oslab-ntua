#ifndef CLIENT_H__
#define CLIENT_H__

#define PORT 50543
#define MSG_SIZE 100
#define TCP_BACKLOG 5
#define MAX_CLIENTS 100


void client(int, char *);
void server(int);

#endif