#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/sysinfo.h>
#include <openssl/err.h>
#include "list.h"
#include "vessel.h"
#include "networking.h"


struct server_conf instance;


/* Handle new connection, create a a fresh new Client structure and link it
   to the fd, ready to be set in EPOLLIN event */
static int accept_handler(Client *server) {

    const int fd = server->fd;

    /* Accept the connection */
    int clientsock = accept_connection(fd);

    /* Abort if not accepted */
    if (clientsock == -1)
        return -1;

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    if (getpeername(clientsock, (struct sockaddr *) &addr, &addrlen) < 0) {
        return -1;
    }

    char ip_buff[INET_ADDRSTRLEN + 1];
    if (inet_ntop(AF_INET, &addr.sin_addr, ip_buff, sizeof(ip_buff)) == NULL) {
        return -1;
    }

    /* Create a server structure to handle his context connection */
    Client *client = malloc(sizeof(Client));
    if (!client) {
        perror("creating client during accept");
        exit(EXIT_FAILURE);
    }

    client->addr = strdup(ip_buff);
    client->fd = clientsock;
    client->epollfd = server->epollfd;
    client->reply = malloc(sizeof(Reply));
    client->ctx_in = server->ctx_in;
    client->ctx_out = server->ctx_out;

    if (instance.encryption == 1) {
        client->ssl = SSL_new(server->ssl_ctx);
        SSL_set_fd(client->ssl, clientsock);

        if (SSL_accept(client->ssl) <= 0) {
            ERR_print_errors_fp(stderr);
        }
    }

    add_client(client);

    /* clientsock = SSL_get_fd(client->ssl); */
    add_epoll(server->epollfd, clientsock, client);

    /* Rearm server fd to accept new connections */
    mod_epoll(server->epollfd, fd, EPOLLIN, server);

    return 0;
}

/* Main worker function, his responsibility is to wait on events on a shared
   EPOLL fd, use the same way for clients or peer to distribute messages */
static void *worker(void *args) {

    struct socks *fds = (struct socks *) args;
    struct epoll_event *evs = malloc(sizeof(*evs) * MAX_EVENTS);

    if (!evs) {
        perror("malloc(3) failed");
        pthread_exit(NULL);
    }

    int events_cnt = 0;

    /* Start looping through FDs for READ/WRITE events */
    while ((events_cnt = epoll_wait(fds->epollfd,
                    evs, instance.epoll_max_events, -1)) > 0) {

        for (int i = 0; i < events_cnt; i++) {

            /* Check for errors first */
            if ((evs[i].events & EPOLLERR) ||
                    (evs[i].events & EPOLLHUP) ||
                    (!(evs[i].events & EPOLLIN) && !(evs[i].events & EPOLLOUT))) {

                /* An error has occured on this fd, or the socket is not
                   ready for reading */
                perror ("epoll_wait(2)");
                close(evs[i].data.fd);

                continue;

            } else if (evs[i].data.fd == instance.event_fd) {

                /* And quit event after that */
                eventfd_t val;
                eventfd_read(instance.event_fd, &val);

                goto exit;

            } else if (evs[i].events & EPOLLIN) {

                Client * c = (Client *) evs[i].data.ptr;

                if (c->fd == fds->serversock) {
                    c->ctx_accept(evs[i].data.ptr);
                } else {
                    /* Finally handle the request according to its type */
                    c->ctx_in(evs[i].data.ptr);
                    mod_epoll(fds->epollfd, c->fd, EPOLLOUT, evs[i].data.ptr);
                }
            } else {
                Client * c = (Client *) evs[i].data.ptr;
                c->ctx_out(evs[i].data.ptr);
                /* Rearm socket for READ event */
                mod_epoll(fds->epollfd, c->fd, EPOLLIN, evs[i].data.ptr);
            }
        }
    }

exit:
    if (events_cnt == 0 && instance.event_fd == 0)
        perror("epoll_wait(2) error");

    free(evs);

    return NULL;
}


void add_client(Client *c) {
    list_push(instance.clients, c);
}

/*
 * Main entry point for start listening on a socket and running an epoll event
 * loop his main responsibility is to pass incoming client connections
 * descriptor to workers thread.
 */
int server(const char *addr, const char *port, Client *server) {

    /* Initialize epollfd for server component */
    const int epollfd = epoll_create1(0);

    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    /* Initialize the sockets, first the server one */
    const int fd = make_listen(addr, port);

    if (server->fd == -1) server->fd = fd;

    if (server->epollfd == -1) server->epollfd = epollfd;

    if (instance.encryption == 1) {
        openssl_init();
        server->ssl_ctx = create_ssl_context();
        load_certificates(server->ssl_ctx, instance.certfile, instance.keyfile);
    }

    /* Set socket in EPOLLIN flag mode, ready to read data */
    add_epoll(epollfd, fd, server);

    /* Add event fd to epoll */
    struct epoll_event ev;
    ev.data.fd = instance.event_fd;
    ev.events = EPOLLIN;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, instance.event_fd, &ev) < 0) {
        perror("epoll_ctl(2): add epollin");
    }

    /* Worker thread pool */
    pthread_t workers[instance.epoll_workers - 1];

    /* I/0 thread pool initialization, passing a the pair {epollfd, fd} sockets
       for each one. Every worker handle input from clients, accepting
       connections and sending out data when a socket is ready to write */
    struct socks fds = { epollfd, fd };

    for (int i = 0; i < instance.epoll_workers - 1; ++i)
        pthread_create(&workers[i], NULL, worker, (void *) &fds);

    /* Use main thread as a worker too */
    worker(&fds);

    for (int i = 0; i < instance.epoll_workers - 1; ++i)
        pthread_join(workers[i], NULL);

    if (instance.encryption == 1) {
        SSL_CTX_free(server->ssl_ctx);
        SSL_free(server->ssl);
        openssl_cleanup();
    }

    close(fd);

    return 0;
}


int start_server(Config *conf) {

    int r = 0;

    Server s = {
        .addr = conf->addr,
        .fd = -1,
        .epollfd = -1,
        .ctx_in = conf->req_handler,
        .ctx_out = conf->rep_handler,
        .reply = NULL,
        .ssl_ctx = NULL,
        .ssl = NULL
    };

    /* Init global configuration, starting with clients list */
    instance.clients = list_init();

    /* Event fd to interrupt epoll wait inside workers */
    instance.event_fd = eventfd(0, EFD_NONBLOCK);

    /* Register epoll_workers, number of thread workers */
    instance.epoll_workers = conf->epoll_workers;

    /* Register max epoll events number */
    instance.epoll_max_events = conf->epoll_events;

    /* Fallback to 64 if not set */
    if (instance.epoll_max_events == -1)
        instance.epoll_max_events = MAX_EVENTS;

    instance.encryption = conf->use_ssl;

    /* Set certificates and key in case of SSL server */
    if (conf->use_ssl) {
        instance.certfile = conf->certfile;
        instance.keyfile = conf->keyfile;
    }

    /* If epoll workers number is not set (e.g. -1) set it to the # of core of
       the machine */
    if (conf->epoll_workers == -1) {
        conf->epoll_workers = get_nprocs();
    }

    /* Fallback to default accept_handler */
    if (!conf->acc_handler) {
        s.ctx_accept = accept_handler;
    }

    /* Run server, blocking call */
    r = server(conf->addr, conf->port, &s);

    /* Free allocated resources */
    for (ListNode *n = instance.clients->head; n != NULL; n = n->next) {
        Client *c = (Client *) n->data;

        if (conf->use_ssl == 1)
            SSL_free(c->ssl);

        close(c->fd);
        free(c->reply);
        free((void *) c->addr);
    }

    list_free(instance.clients, 1);

    return r;
}


void stop_server() {
    for (int i = 0; i < instance.epoll_workers; i++) {
        eventfd_write(instance.event_fd, 1);
        usleep(1500);
    }
}
