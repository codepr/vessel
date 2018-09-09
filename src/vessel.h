#ifndef VESSEL_H
#define VESSEL_H

#include <stdint.h>
#include <openssl/ssl.h>
#include "list.h"


#define MAX_EVENTS	  64


typedef struct client Client;
typedef struct client Server;

typedef struct reply Reply;

struct client {
    const char *addr;
    int fd;
    int epollfd;
    int (*ctx_accept)(Client *);
    int (*ctx_in)(Client *);
    int (*ctx_out)(Client *);
    union {
        Reply *reply;
        void *ptr;
    };
    SSL_CTX *ssl_ctx;
    SSL *ssl;
};


struct socks {
    int epollfd;
    int serversock;
};


struct reply {
    int fd;
    uint8_t *data;
};


typedef struct config {
    /* Epoll events count */
    int epoll_events;
    /* Epoll workers count */
    int epoll_workers;
    const char *addr;
    const char *port;
    int use_ssl;
    const char *certfile;
    const char *keyfile;
    int (*acc_handler)(Client *);
    int (*req_handler)(Client *);
    int (*rep_handler)(Client *);
} Config;


struct server_conf {
    /* Eventfd to break the epoll_wait loop in case of signals */
    int event_fd;
    /* Epoll workers count */
    int epoll_workers;
    /* Epoll max number of events */
    int epoll_max_events;
    /* List of connected clients */
    List *clients;
    /* Certificate file path on the filesystem */
    const char *certfile;
    /* Key file path on the filesystem */
    const char *keyfile;
    /* Encryption flag */
    int encryption;
};

/* Global instance configuration */
extern struct server_conf instance;

/* Start the server, based on the configuration parameters passed, blocking
   call */
int start_server(Config *);

/* Stop the server running by using epoll_workers number of eventfd call, this
   way it will stop all running threads */
void stop_server();

/* Add a connected client to the global instance configuration */
void add_client(Client *);

/* Run the serveri instance, accept addr, port and a Client structure pointer */
int server(const char *, const char *, Client *);


#endif
