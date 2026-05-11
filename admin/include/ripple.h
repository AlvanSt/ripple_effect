#ifndef RIPPLE_H
#define RIPPLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <time.h>

#define MAX_BOARD_SIZE      9
#define MAX_PLAYERS         8
#define MAX_ROOMS           64
#define MAX_ROOMS_PER_BOARD 64
#define TOKEN_LEN           32
#define PARTY_CODE_LEN      6

typedef struct {
    int  size;
    int  cells[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
    int  given[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
    int  rooms[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
    int  room_sizes[MAX_ROOMS_PER_BOARD + 1];
    int  room_count;
    char title[64];
    int  puzzle_id;
} Board;

typedef struct {
    int  id;
    char username[64];
    char password_hash[65];
    int  best_times[MAX_BOARD_SIZE + 1];
    int  total_wins;
    int  total_games;
} User;

typedef struct {
    char token[TOKEN_LEN * 2 + 1];
    int  user_id;
    long expires_at;
} Session;

typedef struct {
    char   code[PARTY_CODE_LEN + 1];
    int    host_id;
    int    puzzle_id;
    int    player_ids[MAX_PLAYERS];
    int    player_count;
    Board  board;                      /* initial shared board (preset) */
    int    status;
    time_t start_time;
    pthread_mutex_t lock;

    /* Leaderboard */
    int    player_status[MAX_PLAYERS];      /* 0 solving, 1 finished */
    time_t player_finish_time[MAX_PLAYERS];
    char   player_names[MAX_PLAYERS][64];

    /* Isolated per‑player boards */
    Board  player_boards[MAX_PLAYERS];
    int    player_board_ready[MAX_PLAYERS]; /* 1 when board is initialised */
} Room;

extern Room              g_rooms[MAX_ROOMS];
extern int               g_room_count;
extern pthread_mutex_t   g_rooms_lock;
extern volatile sig_atomic_t g_running;

#endif /* RIPPLE_H */
