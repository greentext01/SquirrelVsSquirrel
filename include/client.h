#pragma once

#include "tinycthread.h"
#include <stdbool.h>

// Networkig client
typedef struct Client {
    bool running;
    int sockfd;
    int qsize;
    char *queue;
    mtx_t mutex;
    thrd_t recv_thread;
} Client;

// Send entire message
int clientSendAll(Client *self, char *data, int length);
// Recieve all messages
char *clientRecvAll(Client *self);
// Connect to server
int clientConnect(Client *self, char *hostname, int port);
// Start client
void clientStart(Client *self);
// Stop client
void clientStop(Client *self);
// Destroy client
void clientFree(Client *self);
// Free client
Client *clientInit();
