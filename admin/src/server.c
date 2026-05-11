#include <sys/socket.h>
#include "server.h"
#include "ripple.h"
#include "auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <pthread.h>

static int g_server_fd  = -1;
static int g_epoll_fd   = -1;
static pthread_t g_accept_thread;

static void *accept_loop(void *arg) {
    (void)arg;
    struct epoll_event events[64];
    epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, g_server_fd,
              &(struct epoll_event){.events=EPOLLIN, .data.fd=g_server_fd});

    while (g_running) {
        int nfds = epoll_wait(g_epoll_fd, events, 64, 1000);
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == g_server_fd) {
                struct sockaddr_in cli;
                socklen_t len = sizeof(cli);
                int cfd = accept(g_server_fd, (struct sockaddr *)&cli, &len);
                if (cfd < 0) continue;
                PlayerConn *pc = calloc(1, sizeof(PlayerConn));
                pc->socket_fd = cfd;
                pc->user_id   = -1; /* set after auth handshake */
                pthread_create(&pc->thread, NULL, player_thread, pc);
                pthread_detach(pc->thread);
            }
        }
    }
    return NULL;
}

int server_start(int port) {
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };
    bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(g_server_fd, 16);
    g_epoll_fd = epoll_create1(0);
    pthread_create(&g_accept_thread, NULL, accept_loop, NULL);
    fprintf(stdout, "[Server] Listening on port %d\n", port);
    return 0;
}

void server_stop(void) {
    g_running = 0;
    pthread_join(g_accept_thread, NULL);
    close(g_server_fd);
    close(g_epoll_fd);
}

void server_broadcast(Room *room, const char *msg) {
    /* TODO: send msg to all connected sockets in room */
    (void)room; (void)msg;
}

void server_send(PlayerConn *pc, const char *msg) {
    send(pc->socket_fd, msg, strlen(msg), 0);
}
