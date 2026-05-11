#ifndef SERVER_H
#define SERVER_H
#include "ripple.h"
typedef struct {
    int socket_fd;
    int user_id;
    char session_token[65];
    Room *room;
    pthread_t thread;
} PlayerConn;

int  server_start(int port);
void server_stop(void);
void *player_thread(void *arg);
void  server_broadcast(Room *room, const char *msg);
void  server_send(PlayerConn *pc, const char *msg);
#endif
