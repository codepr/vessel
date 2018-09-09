Vessel
======

```c
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <resolv.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "networking.h"
#include "vessel.h"


#define ONEMB 1024 * 1024


static int reply_handler(Client *);
static int request_handler(Client *);


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


int main(int argc, char **argv) {

    Config plain_conf = {
        .epoll_events = 64,
        .epoll_workers = 4,
        .addr = "127.0.0.1",
        .port = "4040",
        .use_ssl = 0,
        .acc_handler = NULL,
        .req_handler = request_handler,
        .rep_handler = reply_handler
        };

    start_server(&plain_conf);

    return 0;
}

```
