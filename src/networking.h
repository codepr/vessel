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


#ifndef NETWORKING_H
#define NETWORKING_H

#include <openssl/ssl.h>
#include "ringbuf.h"


#define BUFSIZE 256


/*
 * Create a non-blocking socket and make it listen on the specfied address and
 * port
 */
int make_listen(const char *, const char *);

/* Accept a connection and add it to the right epollfd */
int accept_connection(const int);

/* Epoll management functions.

   Add and FD or a struct pointer to the EPOLL instance loop running in
   EDGE-TRIGGERED mode, with EPOLLONESHOT flag on, this way EPOLL_WAIT on read
   or write events will wake up just one thread instead of waking up all of
   them causing race-conditions difficult to handle. The downside is that it
   has to be manually re-armed each time an event is triggered. Being struct
   epoll_event FD or data mutually exclusive in an union, if NULL is passed as
   last argument the FD will be registered instead, so if there's need of a
   structure with additional info as well as an FD, it should be stored in the
   structure alongside other info needed */
void add_epoll(const int, const int, void *);

/* Modify state of an already watched FD on the EPOLL event loop, as add_epoll,
   accept a pointer to a struct that can be passed instead of jsut the FD */
void mod_epoll(const int, const int, const int, void *);

/* I/O management functions */
/* Send all data, eventually with multiple send call, last argument, beside
   ordinary args for send call, is a pointer to an integer, referring the
   number of bytes sent at every call, useful to track remaining bytes to
   send out */
int sendall(const int, uint8_t *, ssize_t, ssize_t *);

/* Recv all data, eventually with multiple recv call exactly like sendall,
   instead of using a buffer to recv, it requires a ringbuffer, this way
   it is possible to fill the buffer with subsequent calls and empty it at
   please */
int recvall(const int, Ringbuf *, ssize_t);

/* SSL/TLS versions */
SSL_CTX *create_ssl_context(void);

/* Init openssl library */
void openssl_init(void);

/* Release resources allocated by openssl library */
void openssl_cleanup(void);

/* Load cert.pem and key.pem certfiles from filesystem */
void load_certificates(SSL_CTX *, const char *, const char *);

/* Send data like sendall but adding encryption SSL */
int ssl_send(SSL *, uint8_t *, ssize_t, ssize_t *);

/* Recv data like recvall but adding encryption SSL */
int ssl_recv(SSL *, Ringbuf *, ssize_t);


#endif
