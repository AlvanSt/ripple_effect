#ifndef GAME_H
#define GAME_H
#include "ripple.h"
int  game_is_valid_move(Board *b, int row, int col, int val);
int  game_is_solved(Board *b);
int  game_make_move(Board *b, int row, int col, int val, pthread_mutex_t *lock, sem_t *cell_sems);
void game_reset_board(Board *b);
int  game_create_room(int host_id, int puzzle_id, char *out_code);
Room *game_find_room(const char *code);
int  game_join_room(const char *code, int player_id);
void game_start(Room *room);
void game_finish(Room *room, int winner_id);
#endif
