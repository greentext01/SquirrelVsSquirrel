/****************************************************************
 *  Name: Olivier Audet-Yang        ICS3U        May-June 2024  *
 *                                                              *
 *                        File: client.c                        *
 *                                                              *
 *  Source code for Squirrel vs Squirrel, a squirrel themed     *
 *  and multiplayer Spy vs Spy.                                 *
 ****************************************************************/

// Acknowledgements:
// Beej's guide to network programming for showing me how to use networking
// in C. (https://beej.us/guide/bgnet/)
//
// Craft for proividing a great client-server protocol & program structure
// for games. (https://github.com/fogleman/Craft)
//
// TinyCThread is used as a library to make the C11 threading api portable.
// (https://github.com/tinycthread/tinycthread)

// C library includes
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Defining WIN32_LEAN_AND_MEAN omits winsock from windows.h
// This allows winsock2 to be included separately
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Includes from project
#include "client.h"
#include "tinycthread.h"

// Constants for networking.
#define PORT "3490"
#define RECV_SIZE 4096
#define QUEUE_SIZE 1048576

// Get in_addr or in6_addr pointer from sockaddr, checking IPv4 or IPv6.
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// Send entire message to server, retrying when partial message is sent.
int clientSendAll(Client *self, char *data, int length) {
    int bytesSent = 0;
    int bytesLeft = length;

    while (bytesSent < length) {
        int sent;
        // Returns -1 on error, and number of bytes sent on success.
        if ((sent = send(self->sockfd, data, bytesLeft, 0)) == -1) {
            printf("%d\n", WSAGetLastError());
            perror("send");
            return 1;
        }
        // Substract bytes sent from bytes left
        bytesLeft -= sent;
        bytesSent += sent;
    }

    return 0;
}

// Read all messages from queue (not network).
// By far the most confusing function in the game.
char *clientRecvAll(Client *self) {
    // Resulting message
    char *result = NULL;

    // Lock queue mutex. Prevents race conditions.
    mtx_lock(&self->mutex);

    // Pointer to beginning of last message
    // Start at end of queue
    char *p = self->queue + self->qsize - 1;
    // Back up pointer until end of the end of the last message is reached.
    // If there are no messages, p will end up being queue - 1 (notice the >=)
    while (p >= self->queue && *p != '\n') {
        p--;
    }

    // This code is pretty confusing, so here's a graphic of the queue to make
    // it clearer. |message message message \n incoming message|
    //  ^- Read all these          ^- Ignore partial messages

    // If p is smaller than the queue, there are no messages to be read.
    if (p >= self->queue) {
        // Length of the messages
        int length = p - self->queue + 1;
        // Allocate a new result buffer (account for null terminator)
        result = malloc(sizeof(char) * (length + 1));
        // Copy the remaining characters into the result
        memcpy(result, self->queue, sizeof(char) * length);
        // Add a null terminator
        result[length] = '\0';

        // Calculate how many characters have yet to be read
        int remaining = self->qsize - length;
        // Move remaining characters to the start of the queue, getting rid of
        // the read characters in the process
        memmove(self->queue, p + 1, remaining);
        // The queue shrank, so update the queue size.
        self->qsize -= length;
    }
    // Unlock the mutex.
    mtx_unlock(&self->mutex);
    return result;
}

// A function called in a separate thread. Recieves messages from network
// and writes them to queue.
int recvWorker(void *selfVoidPtr) {
    // Arguments must be passed in void pointers when creating a thread.
    // Cast client back from void pointer.
    Client *self = (Client *)selfVoidPtr;

    // Allocate buffer.
    char *data = malloc(sizeof(char) * RECV_SIZE);

    // While the client is running, recieve messages
    while (self->running) {
        // While the client is running, recieve messages
        int length;

        // Length is returned by the recv function. Returns a negative
        // number on error (or if the socket is disconnected).
        if ((length = recv(self->sockfd, data, RECV_SIZE - 1, 0)) <= 0) {
            // Print error if recv failed and the client wasn't stopped
            if (self->running) {
                perror("recv");
                exit(1);
            } else {
                // If the client wasn't running, stop the thread.
                break;
            }
        }

        while (1) {
            int done = 0;
            // Lock the mutex
            mtx_lock(&self->mutex);

            // If the queue has enough space for the message, copy it in.
            // If not, loop until there is enough space (the queue will shrink
            // as messages are read).
            if (self->qsize + length < QUEUE_SIZE) {
                memcpy(self->queue + self->qsize, data,
                       sizeof(char) * (length + 1));
                self->qsize += length;
                done = 1;
            }

            mtx_unlock(&self->mutex);
            if (done) break;

            // Wait until the end of allocated time slice.
            // This waits for the recieving thread to run instad of looping
            // until the time slice ends.
            Sleep(0);
        }
    }

    // Free buffer
    free(data);
    return 0;
}

// Connect to server
int clientConnect(Client *self, char *hostname, int port) {
    // Convert port to string
    char portStr[10];
    sprintf(portStr, "%d", port);

    // Create hints struct. This gives protocol & ip version information to
    // getaddrinfo.
    // The serverInfo pointer is passed to getaddrinfo as a double-pointer
    // This serves as the return value, and will be set to the head of the
    // linked list returned by getaddrinfo.
    // iterator will be later used to loop through the linked list.
    struct addrinfo hints, *serverInfo, *iterator;
    
    // Variable used to store return values to check for errors
    int result;
    // Store returned IP address by getaddrinfo
    char serverIPAddress[INET6_ADDRSTRLEN];

    // Set hints
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Get all ip addresses associated with hostname
    if ((result = getaddrinfo(hostname, portStr, &hints, &serverInfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
        return 1;
    }

    // Loop through all the results and connect to the first we can
    for (iterator = serverInfo; iterator != NULL;
         iterator = iterator->ai_next) {
        // Get server IP address as string for info
        inet_ntop(iterator->ai_family,
                  get_in_addr((struct sockaddr *)iterator->ai_addr),
                  serverIPAddress, sizeof serverIPAddress);

        printf("Attempting to connect to %s\n", serverIPAddress);

        // Try to create a socket. Try another IP on fail.
        if ((self->sockfd = socket(iterator->ai_family, iterator->ai_socktype,
                                   iterator->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        // Try connecting. Try other IP if failed.
        if (connect(self->sockfd, iterator->ai_addr, iterator->ai_addrlen) ==
            -1) {
            closesocket(self->sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    // If no IPs worked, exit.
    if (iterator == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(1);
    }

    // Print final IP
    inet_ntop(iterator->ai_family,
              get_in_addr((struct sockaddr *)iterator->ai_addr),
              serverIPAddress, sizeof serverIPAddress);
    printf("client: connecting to %s\n", serverIPAddress);

    // Free returned linked list
    freeaddrinfo(serverInfo);  // all done with this structure
}

// Start client. Allocate queue, and starts thread.
void clientStart(Client *self) {
    self->running = true;
    self->queue = (char *)calloc(QUEUE_SIZE, sizeof(char));
    self->qsize = 0;
    // Create queue mutex
    mtx_init(&self->mutex, mtx_plain);
    // Start recieving thread
    if (thrd_create(&self->recv_thread, recvWorker, self) != thrd_success) {
        perror("thrd_create");
        exit(1);
    }
}

// Stop client. Stops recieving thread and frees varaibles.
void clientStop(Client *self) {
    // Settting running to false will break out of the recieving loop
    // on the next iteration.
    self->running = false;
    // Disconnect
    closesocket(self->sockfd);
    // Wait until recieving thread exits
    thrd_join(self->recv_thread, NULL);
    // Free variables
    free(self->queue);
    mtx_destroy(&self->mutex);
}

// Free client struct
void clientFree(Client *self) { free(self); }

// Create client
Client *clientInit() {
    // Setup for windows sockets
    WSADATA wsaData;

    // Initialize winsock and check for errors
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(1);
    }

    // Check if Winsock 2.2 is available
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        fprintf(stderr, "Versiion 2.2 of Winsock is not available.\n");
        WSACleanup();
        exit(2);
    }

    printf("Winsock initialized.\n");

    // Run cleanup at program exit
    atexit((void (*)(void))WSACleanup);

    // Allocate client
    Client *out = (Client *)malloc(sizeof(Client));

    // Initialize client
    out->sockfd = 0;
    out->qsize = 0;
    out->queue = 0;
    out->running = false;

    return out;
}
