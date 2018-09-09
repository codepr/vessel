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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <openssl/err.h>
#include "networking.h"


/* Set non-blocking socket */
static int set_nonblocking(const int fd) {

    int flags, result;

    flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        perror("fcntl");
        return -1;
    }

    result = fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    if (result == -1) {
        perror("fcntl");
        return -1;
    }

    return 0;
}

/* Auxiliary function to create a socket and bind it to a hostname:port pair */
static int create_and_bind(const char *host, const char *port) {

    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE
    };

    struct addrinfo *result, *rp;
    int sfd;

    if (getaddrinfo(host, port, &hints, &result) != 0) {
        perror("getaddrinfo error");
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (sfd == -1) continue;

        /* set SO_REUSEADDR so the socket will be reusable after process kill */
        if (setsockopt(sfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR),
                    &(int) { 1 }, sizeof(int)) < 0) {
            perror("Error setting SO_REUSEADDR flag");
        }

        if ((bind(sfd, rp->ai_addr, rp->ai_addrlen)) == 0) {
            /* Succesful bind */
            break;
        }

        close(sfd);
    }

    if (rp == NULL) {
        perror("Could not bind");
        return -1;
    }

    freeaddrinfo(result);
    return sfd;
}


/*
 * Create a non-blocking socket and make it listen on the specfied address and
 * port
 */
int make_listen(const char *host, const char *port) {

    int sfd;

    // First create and bind a socket
    if ((sfd = create_and_bind(host, port)) == -1)
        abort();

    // Make it non-blocking in order to take the fully advantages of
    // Epoll descriptors monitoring
    if ((set_nonblocking(sfd)) == -1)
        abort();

    // Make socket descriptor in listen mode on host:port
    if ((listen(sfd, SOMAXCONN)) == -1) {
        perror("listen");
        abort();
    }

    return sfd;
}


int accept_connection(const int serversock) {

    int clientsock;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    if ((clientsock = accept(serversock,
                    (struct sockaddr *) &addr, &addrlen)) < 0) {
        perror("accept(2)");
        return -1;
    }

    // Make accepted descriptor non-blocking in order to be monitored by epoll
    set_nonblocking(clientsock);

    return clientsock;
}


int sendall(const int sfd, uint8_t *buf, ssize_t len, ssize_t *sent) {

    int total = 0;
    ssize_t bytesleft = len;
    int n = 0;

    while (total < len) {

        n = send(sfd, buf + total, bytesleft, MSG_NOSIGNAL);

        if (n == -1) {

            // No more data ot be read on the current call
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;

            // Something went wrong
            else {
                perror("send(2): error sending data\n");
                break;
            }
        }

        total += n;
        bytesleft -= n;
    }

    // Update the number of bytes sent in the current call, that's why sent
    // argument is passed as pointer
    *sent = total;

    return n == -1 ? -1 : 0;
}


int recvall(const int sfd, Ringbuf *ringbuf, ssize_t len) {

    int n = 0;
    int total = 0;
    int bufsize = BUFSIZE;

    // BUFSIZE is used as placeholder for the first call, in case of remaining
    // data to be read, len indicates remaining number of bytes left
    if (len > 0)
        bufsize = len + 1;

    uint8_t buf[bufsize];

    for (;;) {

        if ((n = recv(sfd, buf, bufsize - 1, 0)) < 0) {

            // No more data ot be read on the current call
            if (errno == EAGAIN || errno == EWOULDBLOCK)  break;

            // Something went wrong
            else {
                perror("recv(2): error reading data");
                return -1;
            }
        }

        if (n == 0) {
            return 0;
        }

        /* Insert all read bytes in the ring buffer */
        // FIXME check the full ring buffer scenario
        ringbuf_bulk_push(ringbuf, buf, n);

        total += n;
    }
    return total;
}


void add_epoll(const int efd, const int fd, void *data) {

    struct epoll_event ev;
    ev.data.fd = fd;

    // Being epoll_event fd and data inside a union, they mutually exclude each
    // other
    if (data)
        ev.data.ptr = data;

    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        perror("epoll_ctl(2): add epollin");
    }
}


void mod_epoll(const int efd, const int fd, const int evs, void *data) {

    struct epoll_event ev;

    // Being epoll_event fd and data inside a union, they mutually exclude each
    // other
    if (data)
        ev.data.ptr = data;

    ev.events = evs | EPOLLET | EPOLLONESHOT;

    if (epoll_ctl(efd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        perror("epoll_ctl(2): set epoll");
    }
}


void openssl_init() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}


void openssl_cleanup() {
    EVP_cleanup();
}


SSL_CTX *create_ssl_context() {

    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

void load_certificates(SSL_CTX *ctx, const char *cert, const char *key) {

    SSL_CTX_set_ecdh_auto(ctx, 1);

    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    /* verify private key */
    if (!SSL_CTX_check_private_key(ctx) ) {
        fprintf(stderr, "Private key does not match the public certificate\n");
        exit(EXIT_FAILURE);
    }
}

int ssl_send(SSL *ssl, uint8_t *buf, ssize_t len, ssize_t *sent) {

    int total = 0;
    ssize_t bytesleft = len;
    int n = 0;

    while (total < len) {

        n = SSL_write(ssl, buf + total, bytesleft);

        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            else {
                perror("SSL_write(2): error sending data\n");
                break;
            }
        }

        total += n;
        bytesleft -= n;

    }

    *sent = total;

    return n == -1 ? -1 : 0;
}


int ssl_recv(SSL *ssl, Ringbuf *ringbuf, ssize_t len) {

    int n = 0;
    int total = 0;
    int bufsize = 256;

    if (len > 0) bufsize = len + 1;

    uint8_t buf[bufsize];

    for (;;) {

        if ((n = SSL_read(ssl, buf, bufsize - 1)) < 0) {

            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            else {
                perror("recv(2): error reading data");
                return -1;
            }
        }

        if (n == 0) {
            return 0;
        }

        /* Insert all read bytes in the ring buffer */
        // FIXME check the full ring buffer scenario
        ringbuf_bulk_push(ringbuf, buf, n);

        total += n;
    }

    return total;
}
