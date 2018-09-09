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
