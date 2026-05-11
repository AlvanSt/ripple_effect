#include "database.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

sqlite3 *g_db = NULL;

int db_open(const char *path) {
    if (sqlite3_open(path, &g_db) != SQLITE_OK) {
        fprintf(stderr, "DB Cannot open: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    return 0;
}

void db_close(void) {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

int db_create_user(const char *username, const char *pwhash) {
    const char *sql = "INSERT INTO users(username, password_hash) VALUES(?,?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pwhash, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_user(const char *username, User *out) {
    const char *sql = "SELECT id,username,password_hash,total_wins,total_games FROM users WHERE username=?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    out->id = sqlite3_column_int(stmt, 0);
    strncpy(out->username, (char*)sqlite3_column_text(stmt, 1), 63);
    out->username[63] = '\0';
    strncpy(out->password_hash, (char*)sqlite3_column_text(stmt, 2), 64);
    out->password_hash[64] = '\0';
    out->total_wins = sqlite3_column_int(stmt, 3);
    out->total_games = sqlite3_column_int(stmt, 4);

    sqlite3_finalize(stmt);
    return 0;
}

int db_update_stats(int userid, int size, int solvetime, int won) {
    (void)size; (void)solvetime;
    const char *sql = "UPDATE users SET total_wins=total_wins+?, total_games=total_games+1 WHERE id=?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, won ? 1 : 0);
    sqlite3_bind_int(stmt, 2, userid);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}

int db_save_session(const char *token, int userid, long expiresat) {
    const char *sql = "INSERT OR REPLACE INTO sessions(token,userid,expires_at) VALUES(?,?,?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, userid);
    sqlite3_bind_int64(stmt, 3, expiresat);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}

int db_get_session(const char *token, Session *out) {
    const char *sql = "SELECT token,userid,expires_at FROM sessions WHERE token=?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    strncpy(out->token, (char*)sqlite3_column_text(stmt, 0), TOKEN_LEN * 2);
    out->token[TOKEN_LEN * 2] = '\0';
    out->user_id = sqlite3_column_int(stmt, 1);
    out->expires_at = (long)sqlite3_column_int64(stmt, 2);

    sqlite3_finalize(stmt);
    return 0;
}

int db_delete_session(const char *token) {
    const char *sql = "DELETE FROM sessions WHERE token=?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}

int db_save_puzzle(Board *b, int authorid, int isverified) { (void)b; (void)authorid; (void)isverified; return 0; }
int db_get_puzzle(int puzzleid, Board *out) { (void)puzzleid; (void)out; return 0; }
int db_list_gallery(int offset, int limit, Board *out, int *count) { (void)offset; (void)limit; (void)out; (void)count; return 0; }
int db_rate_puzzle(int puzzleid, int playerid, int stars) { (void)puzzleid; (void)playerid; (void)stars; return 0; }
int db_verify_puzzle(int puzzleid) { (void)puzzleid; return 0; }
int db_delete_puzzle(int puzzleid) { (void)puzzleid; return 0; }
int db_save_game(int playerid, Board *b, int elapsedsec) { (void)playerid; (void)b; (void)elapsedsec; return 0; }
int db_load_game(int playerid, Board *out, int *elapsedsec) { (void)playerid; (void)out; (void)elapsedsec; return 0; }
int db_get_leaderboard(int puzzleid, int limit, int *playerids, int *times) { (void)puzzleid; (void)limit; (void)playerids; (void)times; return 0; }
