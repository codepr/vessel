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

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <resolv.h>
#include <netdb.h>
#include <malloc.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "unit.h"
#include "vessel_test.h"
#include "../src/networking.h"
#include "../src/vessel.h"


#define ONEMB 1024 * 1024


static int reply_handler(Client *);
static int request_handler(Client *);
static int reply_ssl_handler(Client *);
static int request_ssl_handler(Client *);


static Config plain_conf = {
    .epoll_events = 64,
    .epoll_workers = 4,
    .addr = "127.0.0.1",
    .port = "4040",
    .use_ssl = 0,
    .acc_handler = NULL,
    .req_handler = request_handler,
    .rep_handler = reply_handler
};


static Config ssl_conf = {
    .epoll_events = 64,
    .epoll_workers = 4,
    .addr = "127.0.0.1",
    .port = "14040",
    .use_ssl = 1,
    .certfile = "cert.pem",
    .keyfile = "key.pem",
    .acc_handler = NULL,
    .req_handler = request_ssl_handler,
    .rep_handler = reply_ssl_handler
};


static int make_connection(const char *hostname, int port) {   int sd;

    struct hostent *host;
    struct sockaddr_in addr;

    if ((host = gethostbyname(hostname)) == NULL) {
        perror(hostname);
        abort();
    }

    sd = socket(PF_INET, SOCK_STREAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = *(long *) (host->h_addr);

    if (connect(sd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
        close(sd);
        perror(hostname);
        abort();
    }
    return sd;
}


static SSL_CTX* init_CTX(void) {

    const SSL_METHOD *method;
    SSL_CTX *ctx;

    OpenSSL_add_all_algorithms();  /* Load cryptos, et.al. */
    SSL_load_error_strings();   /* Bring in and register error messages */
    method = SSLv23_client_method();  /* Create new client-method instance */
    ctx = SSL_CTX_new(method);   /* Create new context */

    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}


static int reply_ssl_handler(Client *client) {

    Reply *r = client->reply;
    ssize_t sent = 0;

    if (!r->data) return -1;

    if ((ssl_send(client->ssl, r->data, strlen((char *) r->data), &sent)) < 0) {
        perror("send(2): can't write on socket descriptor");
    }

    free(client->reply->data);

    return 0;
}

/* Handle incoming requests, after being accepted or after a reply */
static int request_ssl_handler(Client *client) {

    const int clientfd = client->fd;

    /* Buffer to initialize the ring buffer, used to handle input from client */
    uint8_t buffer[ONEMB * 2];

    /* Ringbuffer pointer struct, helpful to handle different and unknown
       size of chunks of data which can result in partially formed packets or
       overlapping as well */
    Ringbuf *rbuf = ringbuf_init(buffer, ONEMB * 2);

    /* Read all data to form a packet flag */
    int read_all = -1;
    size_t bytes;

    /* SSL_accept(client->ssl); */

    if ((bytes = ssl_recv(client->ssl, rbuf, read_all)) < 0) {
        return -1;
    }

    client->reply->data = NULL;

    if (ringbuf_size(rbuf) > 0) {

        uint8_t tmp[ringbuf_size(rbuf)+1];

        ringbuf_bulk_pop(rbuf, tmp, bytes);

        tmp[bytes] = '\0';

        printf("%s\n", tmp);

        client->reply->fd = clientfd;
        client->reply->data = (uint8_t *) strdup((const char *) tmp);
    }

    /* Free ring buffer as we alredy have all needed informations in memory */
    ringbuf_free(rbuf);

    return 0;
}


static int reply_handler(Client *client) {

    Reply *r = client->reply;
    ssize_t sent = 0;

    if ((sendall(r->fd, r->data, strlen((char *) r->data), &sent)) < 0) {
        perror("send(2): can't write on socket descriptor");
    }

    free(client->reply->data);

    return 0;
}

/* Handle incoming requests, after being accepted or after a reply */
static int request_handler(Client *client) {
    const int clientfd = client->fd;

    /* Buffer to initialize the ring buffer, used to handle input from client */
    uint8_t buffer[ONEMB * 2];

    /* Ringbuffer pointer struct, helpful to handle different and unknown
       size of chunks of data which can result in partially formed packets or
       overlapping as well */
    Ringbuf *rbuf = ringbuf_init(buffer, ONEMB * 2);

    /* Read all data to form a packet flag */
    int read_all = -1;
    size_t bytes;

    if ((bytes = recvall(clientfd, rbuf, read_all)) < 0) {
        return -1;
    }

    uint8_t tmp[ringbuf_size(rbuf)+1];

    ringbuf_bulk_pop(rbuf, tmp, bytes);

    tmp[bytes] = '\0';

    printf("%s\n", tmp);

    client->reply->fd = clientfd;
    client->reply->data = (uint8_t *) strdup((char *) tmp);

    /* Free ring buffer as we alredy have all needed informations in memory */
    ringbuf_free(rbuf);

    return 0;
}


static void *start_ssl_server(void *x) {
    start_server(&ssl_conf);
    return NULL;
}


static void *start_plain_server(void *x) {
    start_server(&plain_conf);
    return NULL;
}


static char *start_ssl_client(const char *hostname, const char *portnum) {

    SSL_CTX *ctx;
    int server;
    SSL *ssl;
    char buf[6];
    char *sendstring = "HELLO";
    int bytes;

    ctx = init_CTX();
    server = make_connection(hostname, atoi(portnum));
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, server);

    if (SSL_connect(ssl) == -1) ERR_print_errors_fp(stderr);
    else {

        SSL_write(ssl, sendstring, strlen(sendstring));
        bytes = SSL_read(ssl, buf, sizeof(buf));
        buf[bytes] = 0;

        ASSERT("[! SSL client]: SSL client wrong result", strcmp(buf, sendstring) == 0);

        SSL_free(ssl);        /* release connection state */
    }

    close(server);         /* close socket */
    SSL_CTX_free(ctx);        /* release context */

    return 0;
}


static char *start_plain_client(const char *hostname, const char *portnum) {
    char buf[6];
    char *sendstring = "HELLO";
    ssize_t bytes;

    int server = make_connection(hostname, atoi(portnum));
    sendall(server, (uint8_t *) sendstring, strlen(sendstring), &bytes);
    bytes = recv(server, buf, sizeof(buf), 0);
    buf[bytes] = 0;

    ASSERT("[! Plain client]: Plain client wrong result", strcmp(buf, sendstring) == 0);

    close(server);         /* close socket */

    return 0;
}


char *vessel_plain_test(void) {

    pthread_t plain_server;

    pthread_create(&plain_server, NULL, start_plain_server, NULL);

    usleep(3000);
    start_plain_client("127.0.0.1", "4040");

    stop_server();

    pthread_join(plain_server, NULL);

    return 0;
}


char *vessel_ssl_test(void) {

    pthread_t ssl_server;

    pthread_create(&ssl_server, NULL, start_ssl_server, NULL);

    usleep(3000);
    start_ssl_client("127.0.0.1", "14040");

    stop_server();

    pthread_join(ssl_server, NULL);

    return 0;
}
