#include "server.h"
#include "game.h"
#include "auth.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

/* ── tiny JSON helpers ── */
static int json_str(const char *json, const char *key, char *out, int maxlen) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ') p++;
    if (*p == '"') {
        p++;
        int i = 0;
        while (*p && *p != '"' && i < maxlen - 1) out[i++] = *p++;
        out[i] = '\0';
        return 1;
    }
    return 0;
}

static int json_int(const char *json, const char *key, int *out) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ') p++;
    *out = atoi(p);
    return 1;
}

static void send_json(PlayerConn *pc, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    server_send(pc, buf);
}

/* ── message dispatcher ── */
static void handle_message(PlayerConn *pc, const char *msg) {
    char type[32] = {0};
    if (!json_str(msg, "type", type, sizeof(type))) return;

    /* ── register ── */
    if (strcmp(type, "register") == 0) {
        char user[64]={0}, pass[64]={0};
        json_str(msg, "username", user, 64);
        json_str(msg, "password", pass, 64);
        if (auth_register(user, pass) == 0)
            send_json(pc, "{\"type\":\"register_ok\"}");
        else
            send_json(pc, "{\"type\":\"register_fail\",\"message\":\"Username taken\"}");

    /* ── login ── */
    } else if (strcmp(type, "login") == 0) {
        char user[64]={0}, pass[64]={0}, token[65]={0};
        json_str(msg, "username", user, 64);
        json_str(msg, "password", pass, 64);
        if (auth_login(user, pass, token) == 0) {
            User u; db_get_user(user, &u);
            pc->user_id = u.id;
            strncpy(pc->session_token, token, 64);
            send_json(pc,
                "{\"type\":\"login_ok\",\"token\":\"%s\","
                "\"user\":{\"id\":%d,\"username\":\"%s\"}}",
                token, u.id, u.username);
        } else {
            send_json(pc, "{\"type\":\"login_fail\"}");
        }

    /* ── logout ── */
    } else if (strcmp(type, "logout") == 0) {
        auth_logout(pc->session_token);
        pc->user_id = -1;

    /* ── create_room ── */
    } else if (strcmp(type, "create_room") == 0) {
        char code[PARTY_CODE_LEN + 1] = {0};
        if (game_create_room(pc->user_id, 1, code) == 0)
            send_json(pc, "{\"type\":\"room_created\",\"code\":\"%s\"}", code);
        else
            send_json(pc, "{\"type\":\"error_msg\",\"message\":\"Cannot create room\"}");

    /* ── join_room ── */
    } else if (strcmp(type, "join_room") == 0) {
        char code[PARTY_CODE_LEN + 1] = {0};
        json_str(msg, "code", code, PARTY_CODE_LEN + 1);
        Room *r = game_find_room(code);
        if (!r) {
            send_json(pc, "{\"type\":\"error_msg\",\"message\":\"Room not found\"}");
            return;
        }
        game_join_room(code, pc->user_id);
        pc->room = r;
        /* send current board state */
        send_json(pc, "{\"type\":\"room_created\",\"code\":\"%s\"}", code);

    /* ── start_game ── */
    } else if (strcmp(type, "start_game") == 0) {
        int puzzle_id = 1;
        json_int(msg, "puzzle_id", &puzzle_id);
        char code[PARTY_CODE_LEN + 1] = {0};
        game_create_room(pc->user_id, puzzle_id, code);
        Room *r = game_find_room(code);
        if (!r) { send_json(pc, "{\"type\":\"error_msg\",\"message\":\"Failed\"}"); return; }
        pc->room = r;
        game_start(r);
        /* send board as JSON */
        Board *b = &r->board;
        char cells[512]="[", given[512]="[";
        for (int row=0;row<b->size;row++){
            strcat(cells,"["); strcat(given,"[");
            for(int col=0;col<b->size;col++){
                char tmp[8];
                snprintf(tmp,sizeof(tmp),"%d%s",b->cells[row][col],col<b->size-1?",":"");
                strcat(cells,tmp);
                snprintf(tmp,sizeof(tmp),"%d%s",b->given[row][col],col<b->size-1?",":"");
                strcat(given,tmp);
            }
            strcat(cells,row<b->size-1?"]," : "]");
            strcat(given,row<b->size-1?"]," : "]");
        }
        strcat(cells,"]"); strcat(given,"]");
        send_json(pc,
            "{\"type\":\"game_start\",\"board\":"
            "{\"size\":%d,\"cells\":%s,\"given\":%s}}",
            b->size, cells, given);

    /* ── move ── */
    } else if (strcmp(type, "move") == 0) {
        if (!pc->room) { send_json(pc, "{\"type\":\"error_msg\",\"message\":\"Not in a room\"}"); return; }
        int row=0, col=0, value=0;
        json_int(msg, "row",   &row);
        json_int(msg, "col",   &col);
        json_int(msg, "value", &value);
        Board *b = &pc->room->board;
        sem_t dummy; /* TODO: replace with real per-cell semaphores */
        sem_init(&dummy, 0, 1);
        if (game_make_move(b, row, col, value, &pc->room->lock, &dummy)) {
            send_json(pc,
                "{\"type\":\"move_ok\",\"row\":%d,\"col\":%d,\"value\":%d}",
                row, col, value);
            if (game_is_solved(b)) {
                send_json(pc,
                    "{\"type\":\"game_won\",\"winner\":\"%s\",\"time\":0}",
                    pc->user_id >= 0 ? "You" : "Guest");
            }
        } else {
            send_json(pc, "{\"type\":\"invalid_move\"}");
        }
        sem_destroy(&dummy);

    /* ── hint ── */
    } else if (strcmp(type, "hint") == 0) {
        send_json(pc, "{\"type\":\"hint_response\",\"row\":0,\"col\":0,\"value\":1}");

    /* ── save_game ── */
    } else if (strcmp(type, "save_game") == 0) {
        if (pc->room && pc->user_id >= 0) {
            long elapsed = (long)(time(NULL) - pc->room->start_time);
            db_save_game(pc->user_id, &pc->room->board, (int)elapsed);
        }
        send_json(pc, "{\"type\":\"save_ok\"}");

    /* ── leave_room ── */
    } else if (strcmp(type, "leave_room") == 0) {
        pc->room = NULL;

    /* ── gallery_list ── */
    } else if (strcmp(type, "gallery_list") == 0) {
        send_json(pc, "{\"type\":\"gallery_data\",\"puzzles\":[]}");
    }
}

void *player_thread(void *arg) {
    PlayerConn *pc = (PlayerConn *)arg;
    char buf[2048];
    ssize_t n;
    while ((n = recv(pc->socket_fd, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        handle_message(pc, buf);
    }
    if (pc->room) pc->room = NULL;
    close(pc->socket_fd);
    free(pc);
    return NULL;
}
