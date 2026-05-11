#include "game.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int game_is_valid_move(Board *b, int row, int col, int val) {
    if (val < 1 || val > b->size) return 0;
    int rid = b->rooms[row][col];
    if (val > b->room_sizes[rid]) return 0;
    for (int r=0; r<b->size; r++)
        for (int c=0; c<b->size; c++)
            if (b->rooms[r][c]==rid && (r!=row||c!=col) && b->cells[r][c]==val) return 0;
    for (int c=0; c<b->size; c++)
        if (c!=col && b->cells[row][c]==val && abs(col-c)<=val) return 0;
    for (int r=0; r<b->size; r++)
        if (r!=row && b->cells[r][col]==val && abs(row-r)<=val) return 0;
    return 1;
}

int game_is_solved(Board *b) {
    for (int r=0; r<b->size; r++)
        for (int c=0; c<b->size; c++)
            if (b->cells[r][c]==0) return 0;
    for (int r=0; r<b->size; r++)
        for (int c=0; c<b->size; c++)
            if (!game_is_valid_move(b,r,c,b->cells[r][c])) return 0;
    return 1;
}

int game_make_move(Board *b, int row, int col, int val, pthread_mutex_t *lock, sem_t *cell_sems) {
    sem_wait(&cell_sems[row * MAX_BOARD_SIZE + col]);
    pthread_mutex_lock(lock);
    if (!b->given[row][col] && game_is_valid_move(b, row, col, val)) {
        b->cells[row][col] = val;
        pthread_mutex_unlock(lock);
        sem_post(&cell_sems[row * MAX_BOARD_SIZE + col]);
        return 1;
    }
    pthread_mutex_unlock(lock);
    sem_post(&cell_sems[row * MAX_BOARD_SIZE + col]);
    return 0;
}

void game_reset_board(Board *b) {
    for (int r=0; r<b->size; r++)
        for (int c=0; c<b->size; c++)
            if (!b->given[r][c]) b->cells[r][c] = 0;
}

static void gen_party_code(char *out) {
    const char *alpha = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    int fd = open("/dev/urandom", O_RDONLY);
    unsigned char buf[PARTY_CODE_LEN];
    read(fd, buf, PARTY_CODE_LEN);
    close(fd);
    for (int i=0; i<PARTY_CODE_LEN; i++) out[i] = alpha[buf[i] % 32];
    out[PARTY_CODE_LEN] = 0;
}

int game_create_room(int hostid, int puzzleid, char *out_code) {
    pthread_mutex_lock(&g_rooms_lock);
    if (g_room_count >= MAX_ROOMS) { pthread_mutex_unlock(&g_rooms_lock); return -1; }
    Room *r = &g_rooms[g_room_count++];
    memset(r, 0, sizeof(*r));
    gen_party_code(r->code);
    r->host_id = hostid; r->puzzle_id = puzzleid; r->status = 0;
    pthread_mutex_init(&r->lock, NULL);
    strncpy(out_code, r->code, PARTY_CODE_LEN+1);
    pthread_mutex_unlock(&g_rooms_lock);
    return 0;
}

Room *game_find_room(const char *code) {
    for (int i=0; i<g_room_count; i++)
        if (strncmp(g_rooms[i].code, code, PARTY_CODE_LEN)==0) return &g_rooms[i];
    return NULL;
}

int game_join_room(const char *code, int playerid) {
    Room *r = game_find_room(code);
    if (!r) return -1;
    pthread_mutex_lock(&r->lock);
    if (r->status != 0 || r->player_count >= MAX_PLAYERS) {
        pthread_mutex_unlock(&r->lock);
        return -1;
    }
    r->player_ids[r->player_count++] = playerid;
    pthread_mutex_unlock(&r->lock);
    return 0;
}

void game_start(Room *r) {
    pthread_mutex_lock(&r->lock);
    r->status = 1; r->start_time = time(NULL);
    pthread_mutex_unlock(&r->lock);
}

void game_finish(Room *r, int winner_id) {
    (void)winner_id;
    pthread_mutex_lock(&r->lock);
    r->status = 2;
    pthread_mutex_unlock(&r->lock);
}

int game_get_room_count(void) { extern int g_room_count; return g_room_count; }
