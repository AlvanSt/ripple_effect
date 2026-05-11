#include "ripple.h"
#include <sys/epoll.h>
#include <stdio.h>

/* Thin helpers around epoll for the accept loop in server.c */
int epoll_add(int epfd, int fd, uint32_t events) {
    struct epoll_event ev = { .events = events, .data.fd = fd };
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

int epoll_del(int epfd, int fd) {
    return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
}
