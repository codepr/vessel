/* BSD 2-Clause License
 *
 * Copyright (c) 2018, Andrea Giacomo Baldan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
