#include "database.h"
#include <stdio.h>

static const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS users ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  username TEXT UNIQUE NOT NULL,"
    "  password_hash TEXT NOT NULL,"
    "  total_wins INTEGER DEFAULT 0,"
    "  total_games INTEGER DEFAULT 0"
    ");"
    "CREATE TABLE IF NOT EXISTS sessions ("
    "  token TEXT PRIMARY KEY,"
    "  userid INTEGER NOT NULL,"
    "  expires_at INTEGER NOT NULL,"
    "  FOREIGN KEY(userid) REFERENCES users(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS puzzles ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  title TEXT NOT NULL,"
    "  author_id INTEGER,"
    "  size INTEGER NOT NULL,"
    "  rooms_json TEXT NOT NULL,"
    "  play_count INTEGER DEFAULT 0,"
    "  avg_rating REAL DEFAULT 0,"
    "  is_verified INTEGER DEFAULT 0,"
    "  FOREIGN KEY(author_id) REFERENCES users(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS ratings ("
    "  puzzle_id INTEGER NOT NULL,"
    "  player_id INTEGER NOT NULL,"
    "  stars INTEGER NOT NULL,"
    "  PRIMARY KEY(puzzle_id, player_id),"
    "  FOREIGN KEY(puzzle_id) REFERENCES puzzles(id),"
    "  FOREIGN KEY(player_id) REFERENCES users(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS leaderboard ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  player_id INTEGER NOT NULL,"
    "  puzzle_id INTEGER NOT NULL,"
    "  solve_time INTEGER NOT NULL,"
    "  timestamp INTEGER NOT NULL,"
    "  FOREIGN KEY(player_id) REFERENCES users(id),"
    "  FOREIGN KEY(puzzle_id) REFERENCES puzzles(id)"
    ");"
    "CREATE TABLE IF NOT EXISTS rooms ("
    "  code TEXT PRIMARY KEY,"
    "  host_id INTEGER,"
    "  puzzle_id INTEGER,"
    "  status INTEGER DEFAULT 0"
    ");"
    "CREATE TABLE IF NOT EXISTS saved_games ("
    "  player_id INTEGER PRIMARY KEY,"
    "  board_state TEXT NOT NULL,"
    "  elapsed_sec INTEGER DEFAULT 0,"
    "  FOREIGN KEY(player_id) REFERENCES users(id)"
    ");";

int db_init_schema(void) {
    char *err = NULL;
    if (sqlite3_exec(g_db, SCHEMA_SQL, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "DB Schema error: %s\n", err);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}
