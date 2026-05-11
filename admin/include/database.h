#ifndef DATABASE_H
#define DATABASE_H
#include <sqlite3.h>
#include "ripple.h"

int  db_open(const char *path);
void db_close(void);
int  db_init_schema(void);

int db_create_user(const char *username, const char *pwhash);
int  db_get_user(const char *username, User *out);
int  db_update_stats(int user_id, int size, int solve_time, int won);

int  db_save_session(const char *token, int user_id, long expires_at);
int  db_get_session(const char *token, Session *out);
int  db_delete_session(const char *token);

int  db_save_puzzle(Board *b, int author_id, int is_verified);
int  db_get_puzzle(int puzzle_id, Board *out);
int  db_list_gallery(int offset, int limit, Board *out, int *count);
int  db_rate_puzzle(int puzzle_id, int player_id, int stars);
int  db_verify_puzzle(int puzzle_id);
int  db_delete_puzzle(int puzzle_id);

int  db_save_game(int player_id, Board *b, int elapsed_sec);
int  db_load_game(int player_id, Board *out, int *elapsed_sec);

int  db_get_leaderboard(int puzzle_id, int limit, int *player_ids, int *times);

extern sqlite3 *g_db;
#endif
