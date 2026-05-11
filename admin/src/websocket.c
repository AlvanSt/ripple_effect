#include "ripple.h"
#include "server.h"
#include "game.h"
#include "auth.h"
#include "database.h"
#include "solver.h"
#include "puzzle.h"
#include <libwebsockets.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

typedef struct {
    int  user_id;
    int  conn_id;
    char username[64];
    char session_token[65];
    Room *room;
    int   is_host;
    char  pending[4096];
    int   pending_len;
    char  out_buf[65536]; /* INCREASED: 64KB Buffer for large 8x8 arrays */
    int   out_len;
    int   out_ready;
} WsSession;

static int g_conn_counter = 0;

static void broadcast_leaderboard(Room *r);
static void handle_msg(struct lws *wsi, WsSession *sess, const char *msg);

static int jstr(const char *j, const char *k, char *out, int max) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", k);
    const char *p = strstr(j, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ' || *p == ':') p++;
    if (*p == '"') p++;
    int i = 0;
    while (*p && *p != '"' && i < max - 1) { out[i++] = *p++; }
    out[i] = 0;
    return 1;
}

static int jint(const char *j, const char *k, int *out) {
    char s[64]; snprintf(s, sizeof(s), "\"%s\":", k);
    const char *p = strstr(j, s);
    if (!p) return 0;
    p += strlen(s); while (*p == ' ') p++;
    *out = atoi(p); return 1;
}

static void append_to_out_buf(WsSession *s, const char *raw) {
    int raw_len = strlen(raw);
    if (s->out_len > 0) {
        if (s->out_len + raw_len + 2 < (int)sizeof(s->out_buf)) {
            s->out_buf[s->out_len] = '\n';
            strcpy(s->out_buf + s->out_len + 1, raw);
            s->out_len += raw_len + 1;
        }
    } else {
        if (raw_len < (int)sizeof(s->out_buf)) {
            strcpy(s->out_buf, raw);
            s->out_len = raw_len;
        }
    }
    s->out_ready = 1;
}

static void ws_queue(struct lws *wsi, WsSession *sess, const char *fmt, ...) {
    char raw[65536]; /* INCREASED to prevent truncation */
    va_list ap; va_start(ap, fmt);
    vsnprintf(raw, sizeof(raw), fmt, ap);
    va_end(ap);

    append_to_out_buf(sess, raw);
    lws_callback_on_writable(wsi);
}

#define MAX_RS 64
static struct { struct lws *wsi; WsSession *sess; Room *room; int conn_id; } g_rs[MAX_RS];
static int g_rs_n = 0;
static pthread_mutex_t g_rs_lock = PTHREAD_MUTEX_INITIALIZER;

static void rs_add(struct lws *wsi, WsSession *sess, Room *r) {
    pthread_mutex_lock(&g_rs_lock);
    if (g_rs_n < MAX_RS) {
        g_rs[g_rs_n].wsi = wsi;
        g_rs[g_rs_n].sess = sess;
        g_rs[g_rs_n].room = r;
        g_rs[g_rs_n].conn_id = sess->conn_id;
        g_rs_n++;
    }
    pthread_mutex_unlock(&g_rs_lock);
}

static void rs_remove(struct lws *wsi) {
    pthread_mutex_lock(&g_rs_lock);
    for (int i=0;i<g_rs_n;i++) if (g_rs[i].wsi==wsi) { g_rs[i]=g_rs[--g_rs_n]; break; }
    pthread_mutex_unlock(&g_rs_lock);
}

static void broadcast_room(Room *r, const char *fmt, ...) {
    char raw[65536];
    va_list ap; va_start(ap,fmt); vsnprintf(raw,sizeof(raw),fmt,ap); va_end(ap);

    pthread_mutex_lock(&g_rs_lock);
    for (int i=0;i<g_rs_n;i++) {
        if (g_rs[i].room==r) {
            append_to_out_buf(g_rs[i].sess, raw);
            lws_callback_on_writable(g_rs[i].wsi);
        }
    }
    pthread_mutex_unlock(&g_rs_lock);
}

static void send_to_conn(Room *r, int conn_id, const char *fmt, ...) {
    char raw[65536];
    va_list ap; va_start(ap,fmt); vsnprintf(raw,sizeof(raw),fmt,ap); va_end(ap);

    pthread_mutex_lock(&g_rs_lock);
    for (int i=0;i<g_rs_n;i++) {
        if (g_rs[i].room==r && g_rs[i].conn_id==conn_id) {
            append_to_out_buf(g_rs[i].sess, raw);
            lws_callback_on_writable(g_rs[i].wsi);
            break;
        }
    }
    pthread_mutex_unlock(&g_rs_lock);
}

Board g_presets[100];
int g_preset_count = 0;

void board_to_json(Board *b, char *out, int maxlen) {
    char cells[4096]="[", givn[4096]="[", rms[4096]="[";
    for (int r=0;r<b->size;r++) {
        strcat(cells,"["); strcat(givn,"["); strcat(rms,"[");
        for (int c=0;c<b->size;c++) {
            char t[8]; int last=(c==b->size-1);
            snprintf(t,8,"%d%s",b->cells[r][c],last?"":","); strcat(cells,t);
            snprintf(t,8,"%d%s",b->given[r][c], last?"":","); strcat(givn,t);
            snprintf(t,8,"%d%s",b->rooms[r][c], last?"":","); strcat(rms,t);
        }
        int lr=(r==b->size-1);
        strcat(cells,lr?"]":"],"); strcat(givn,lr?"]":"],"); strcat(rms,lr?"]":"],");
    }
    strcat(cells,"]"); strcat(givn,"]"); strcat(rms,"]");
    snprintf(out,maxlen,"{\"size\":%d,\"cells\":%s,\"given\":%s,\"rooms\":%s}", b->size,cells,givn,rms);
}

static void load_board(Room *r, int idx) {
    static int initialized = 0;
    if (!initialized) {
        FILE *fp = fopen("admin/assets/puzzle.txt", "r");
        g_preset_count = 0;
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp) && g_preset_count < 100) {
                if (strncmp(line, "SIZE", 4) == 0) {
                    Board *b = &g_presets[g_preset_count];
                    memset(b, 0, sizeof(Board));
                    sscanf(line, "SIZE %d", &b->size);
                    while(fgets(line, sizeof(line), fp)) {
                        if (strncmp(line, "TITLE", 5) == 0) {
                            char *t = line + 5;
                            while (*t == ' ') t++;
                            strncpy(b->title, t, 63);
                            b->title[strcspn(b->title, "\r\n")] = '\0';
                            break;
                        }
                    }
                    while(fgets(line, sizeof(line), fp)) { if (strncmp(line, "ROOMS", 5) == 0) break; }
                    int room_map[256] = {0};
                    int current_room_id = 1;
                    for (int row = 0; row < b->size; row++) {
                        if (fgets(line, sizeof(line), fp)) {
                            char *p = line;
                            for (int col = 0; col < b->size; col++) {
                                while (*p == ' ') p++;
                                if (*p >= 'A' && *p <= 'Z') {
                                    int letter = (int)*p;
                                    if (room_map[letter] == 0) { room_map[letter] = current_room_id++; }
                                    int rid = room_map[letter];
                                    b->rooms[row][col] = rid;
                                    b->room_sizes[rid]++;
                                }
                                p++;
                            }
                        }
                    }
                    b->room_count = current_room_id - 1;
                    while(fgets(line, sizeof(line), fp)) { if (strncmp(line, "GRID", 4) == 0) break; }
                    for (int row = 0; row < b->size; row++) {
                        if (fgets(line, sizeof(line), fp)) {
                            char *p = line;
                            for (int col = 0; col < b->size; col++) {
                                while (*p == ' ') p++;
                                if (*p >= '1' && *p <= '9') {
                                    b->cells[row][col] = *p - '0';
                                    b->given[row][col] = 1;
                                } else if (*p == '.') {
                                    b->cells[row][col] = 0;
                                    b->given[row][col] = 0;
                                }
                                p++;
                            }
                        }
                    }
                    g_preset_count++;
                }
            }
            fclose(fp);
        }
        initialized = 1;
    }

    pthread_mutex_lock(&r->lock);
    if (idx > 0 && idx <= g_preset_count) {
        r->board = g_presets[idx - 1];
    } else {
        if (g_preset_count > 0) {
            r->board = g_presets[rand() % g_preset_count];
        } else {
            r->board = g_presets[0];
        }
    }
    for (int i = 0; i < r->player_count; i++) {
        r->player_boards[i] = r->board;
        r->player_board_ready[i] = 1;
    }
    pthread_mutex_unlock(&r->lock);
}

static void broadcast_leaderboard(Room *r) {
    time_t now = time(NULL);
    char lead[8192] = "{\"type\":\"leaderboard\",\"players\":[";
    int first = 1;
    for (int i = 0; i < r->player_count; i++) {
        const char *name = r->player_names[i][0] ? r->player_names[i] : "Player";
        int status = r->player_status[i];
        long elapsed = 0;
        if (status == 1) {
            elapsed = (long)(r->player_finish_time[i] - r->start_time);
        } else {
            elapsed = (long)(now - r->start_time);
        }
        char entry[256];
        snprintf(entry, sizeof(entry),
            "%s{\"name\":\"%s\",\"status\":%d,\"time\":%ld}",
            first ? "" : ",", name, status, elapsed);
        strcat(lead, entry);
        first = 0;
    }
    strcat(lead, "]}");
    broadcast_room(r, lead);
}

static void *leaderboard_thread(void *arg) {
    (void)arg;
    while (g_running) {
        struct timespec ts;
        ts.tv_sec = 1;
        ts.tv_nsec = 0;
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&g_rooms_lock);
        for (int i = 0; i < g_room_count; i++) {
            Room *r = &g_rooms[i];
            pthread_mutex_lock(&r->lock);
            if (r->status == 1) {
                broadcast_leaderboard(r);
            }
            pthread_mutex_unlock(&r->lock);
        }
        pthread_mutex_unlock(&g_rooms_lock);
    }
    return NULL;
}

static void handle_msg(struct lws *wsi, WsSession *sess, const char *msg) {
    char type[32]={0};
    if (!jstr(msg,"type",type,sizeof(type))) return;

    if (strcmp(type,"register")==0) {
        char u[64]={0},p[64]={0}; jstr(msg,"username",u,64); jstr(msg,"password",p,64);
        if (auth_register(u,p)==0) ws_queue(wsi,sess,"{\"type\":\"register_ok\"}");
        else ws_queue(wsi,sess,"{\"type\":\"register_fail\",\"message\":\"Username taken\"}");

    } else if (strcmp(type,"login")==0) {
        char u[64]={0},p[64]={0},tok[65]={0}; jstr(msg,"username",u,64); jstr(msg,"password",p,64);
        if (auth_login(u,p,tok)==0) {
            User usr; db_get_user(u,&usr);
            sess->user_id=usr.id; strncpy(sess->username,usr.username,63); strncpy(sess->session_token,tok,64);
            ws_queue(wsi,sess,"{\"type\":\"login_ok\",\"token\":\"%s\",\"user\":{\"id\":%d,\"username\":\"%s\"}}",tok,usr.id,usr.username);
        } else { ws_queue(wsi,sess,"{\"type\":\"login_fail\"}"); }

    } else if (strcmp(type,"guest")==0) {
        static int gn=0; gn++; snprintf(sess->username,63,"Guest%d",gn); sess->user_id=-1;
        ws_queue(wsi,sess,"{\"type\":\"guest_ok\",\"username\":\"%s\"}",sess->username);

    } else if (strcmp(type,"logout")==0) {
        auth_logout(sess->session_token); sess->user_id=-1; sess->room=NULL;

    } else if (strcmp(type,"create_room")==0) {
        int pid = 0; jint(msg, "puzzle_id", &pid);
        char code[PARTY_CODE_LEN+1]={0};
        if (game_create_room(sess->user_id,pid,code)==0) {
            Room *r=game_find_room(code);
            pthread_mutex_lock(&r->lock);
            r->player_count = 1;
            r->status = 0;
            r->player_status[0] = 0;
            r->player_finish_time[0] = 0;
            strncpy(r->player_names[0], sess->username, 63);
            r->player_names[0][63] = '\0';
            r->player_board_ready[0] = 0;
            pthread_mutex_unlock(&r->lock);
            sess->room=r; sess->is_host=1; load_board(r,pid); rs_add(wsi,sess,r);
            ws_queue(wsi,sess,"{\"type\":\"room_created\",\"code\":\"%s\",\"is_host\":1,\"players\":1}",code);
        }
    } else if (strcmp(type,"join_room")==0) {
        char code[PARTY_CODE_LEN+1]={0};
        jstr(msg,"code",code,PARTY_CODE_LEN+1);
        Room *r=game_find_room(code);
        if (!r) {
            ws_queue(wsi,sess,"{\"type\":\"error_msg\",\"message\":\"Room not found\"}");
            return;
        }
        int ret = game_join_room(code, sess->user_id);
        if (ret != 0) {
            ws_queue(wsi,sess,"{\"type\":\"error_msg\",\"message\":\"Cannot join room\"}");
            return;
        }
        pthread_mutex_lock(&r->lock);
        int idx = r->player_count - 1;
        r->player_status[idx] = 0;
        r->player_finish_time[idx] = 0;
        strncpy(r->player_names[idx], sess->username, 63);
        r->player_names[idx][63] = '\0';
        r->player_board_ready[idx] = 0;
        pthread_mutex_unlock(&r->lock);
        sess->room = r; sess->is_host = 0;
        rs_add(wsi, sess, r);
        ws_queue(wsi, sess,
            "{\"type\":\"room_joined\",\"code\":\"%s\",\"players\":%d,\"status\":%d}",
            code, r->player_count, r->status);
        broadcast_room(r,
            "{\"type\":\"room_update\",\"players\":%d,\"code\":\"%s\"}",
            r->player_count, code);

    } else if (strcmp(type,"start_party")==0) {
        if (!sess->room||!sess->is_host) return;
        Room *r = sess->room;
        game_start(r);
        pthread_mutex_lock(&r->lock);
        for (int i = 0; i < r->player_count; i++) {
            if (!r->player_board_ready[i]) {
                r->player_boards[i] = r->board;
                r->player_board_ready[i] = 1;
            }
        }
        pthread_mutex_unlock(&r->lock);

        pthread_mutex_lock(&g_rs_lock);
        for (int i = 0; i < g_rs_n; i++) {
            if (g_rs[i].room == r) {
                WsSession *s = g_rs[i].sess;
                int pidx = -1;
                for (int j = 0; j < r->player_count; j++) {
                    if (r->player_names[j][0] && strcmp(r->player_names[j], s->username) == 0) {
                        pidx = j; break;
                    }
                }
                if (pidx < 0) pidx = 0;
                char bj[8192]; board_to_json(&r->player_boards[pidx], bj, sizeof(bj));
                
                snprintf(s->out_buf, sizeof(s->out_buf),
                         "{\"type\":\"game_start\",\"code\":\"%s\",\"board\":%s}", r->code, bj);
                s->out_len = strlen(s->out_buf); s->out_ready = 1;
                lws_callback_on_writable(g_rs[i].wsi);
            }
        }
        pthread_mutex_unlock(&g_rs_lock);

    } else if (strcmp(type,"start_game")==0) {
        int pid=0; jint(msg,"puzzle_id",&pid);
        char code[PARTY_CODE_LEN+1]={0};
        game_create_room(sess->user_id,pid,code);
        Room *r=game_find_room(code);
        if (!r) return;
        pthread_mutex_lock(&r->lock);
        r->player_count = 1; r->status = 1;
        r->player_status[0] = 0; r->player_finish_time[0] = 0;
        strncpy(r->player_names[0], sess->username, 63);
        r->player_names[0][63] = '\0';
        pthread_mutex_unlock(&r->lock);
        sess->room=r; sess->is_host=1; load_board(r,pid); rs_add(wsi,sess,r);
        game_start(r);
        char bj[8192]; board_to_json(&r->board, bj, sizeof(bj));
        
        ws_queue(wsi,sess,"{\"type\":\"game_start\",\"code\":\"\",\"board\":%s}",bj);

    } else if (strcmp(type,"validate_puzzle")==0 || strcmp(type,"publish_puzzle")==0) {
        Board b; memset(&b, 0, sizeof(Board));
        jint(msg, "size", &b.size);
        jstr(msg, "title", b.title, 64);
        
        char cells_flat[1024]={0}, rooms_flat[1024]={0};
        jstr(msg, "cells_flat", cells_flat, sizeof(cells_flat));
        jstr(msg, "rooms_flat", rooms_flat, sizeof(rooms_flat));

        int r=0, c=0;
        char *p = cells_flat;
        while(*p && r < b.size) {
            b.cells[r][c] = atoi(p);
            b.given[r][c] = (b.cells[r][c] > 0) ? 1 : 0;
            c++; if(c == b.size){ c=0; r++; }
            while(*p && *p != ',') p++;
            if(*p == ',') p++;
        }
        r=0; c=0; p=rooms_flat;
        while(*p && r < b.size) {
            b.rooms[r][c] = atoi(p);
            b.room_sizes[b.rooms[r][c]]++;
            c++; if(c == b.size){ c=0; r++; }
            while(*p && *p != ',') p++;
            if(*p == ',') p++;
        }

        if (strcmp(type, "validate_puzzle") == 0) {
            int initial_valid = 1;
            for (int rr = 0; rr < b.size; rr++) {
                for (int cc = 0; cc < b.size; cc++) {
                    if (b.cells[rr][cc] > 0) {
                        int temp = b.cells[rr][cc];
                        b.cells[rr][cc] = 0; 
                        if (!game_is_valid_move(&b, rr, cc, temp)) {
                            initial_valid = 0;
                        }
                        b.cells[rr][cc] = temp;
                    }
                }
            }
            
            if (initial_valid == 0) {
                ws_queue(wsi, sess, "{\"type\":\"validate_fail\"}");
                return;
            }

            Board solved; memset(&solved, 0, sizeof(Board));
            int ok = solver_run_as_child(&b, &solved);
            if (ok == 0 && solved.size > 0) {
                ws_queue(wsi, sess, "{\"type\":\"validate_ok\"}");
            } else {
                ws_queue(wsi, sess, "{\"type\":\"validate_fail\"}");
            }
        } else {
            puzzle_save_file("admin/assets/puzzle.txt", &b);
            if (g_preset_count < 100) {
                g_presets[g_preset_count] = b;
                g_preset_count++;
            }
            ws_queue(wsi, sess, "{\"type\":\"publish_ok\"}");
        }

    } else if (strcmp(type,"move")==0) {
        if (!sess->room || sess->room->status != 1) return;
        int row=0,col=0,value=0;
        jint(msg,"row",&row); jint(msg,"col",&col); jint(msg,"value",&value);
        int pidx = -1;
        Room *room_ptr = sess->room;
        pthread_mutex_lock(&room_ptr->lock);
        for (int i = 0; i < room_ptr->player_count; i++) {
            if (strcmp(room_ptr->player_names[i], sess->username) == 0) { pidx = i; break; }
        }
        if (pidx < 0 || !room_ptr->player_board_ready[pidx]) { pthread_mutex_unlock(&room_ptr->lock); return; }
        Board *b = &room_ptr->player_boards[pidx];
        
        if (row>=0 && row<b->size && col>=0 && col<b->size && !b->given[row][col]) {
            if (value >= 0 && value <= b->size) {
                b->cells[row][col] = value;
                pthread_mutex_unlock(&room_ptr->lock);
                send_to_conn(room_ptr, sess->conn_id,
                    "{\"type\":\"move_ok\",\"row\":%d,\"col\":%d,\"value\":%d}",
                    row, col, value);
            } else {
                pthread_mutex_unlock(&room_ptr->lock);
                send_to_conn(room_ptr, sess->conn_id, "{\"type\":\"invalid_move\"}");
            }
        } else {
            pthread_mutex_unlock(&room_ptr->lock);
        }

    } else if (strcmp(type,"hint")==0) {
        if (!sess->room || sess->room->status != 1) return;
        int pidx = -1;
        Room *room_ptr = sess->room;
        pthread_mutex_lock(&room_ptr->lock);
        for (int i = 0; i < room_ptr->player_count; i++) {
            if (strcmp(room_ptr->player_names[i], sess->username) == 0) { pidx = i; break; }
        }
        if (pidx >= 0 && room_ptr->player_board_ready[pidx]) {
            Board clean = room_ptr->player_boards[pidx];
            for(int rr=0; rr<clean.size; rr++) for(int cc=0; cc<clean.size; cc++) if(!clean.given[rr][cc]) clean.cells[rr][cc] = 0;
            Board solved; memset(&solved, 0, sizeof(Board));
            
            if (solver_run_as_child(&clean, &solved) == 0 && solved.size > 0) {
                int hint_r = -1, hint_c = -1, hint_v = -1;
                int start_idx = rand() % (clean.size * clean.size);
                for (int i = 0; i < clean.size * clean.size; i++) {
                    int idx = (start_idx + i) % (clean.size * clean.size);
                    int rr = idx / clean.size;
                    int cc = idx % clean.size;
                    if (room_ptr->player_boards[pidx].cells[rr][cc] == 0 && !room_ptr->player_boards[pidx].given[rr][cc]) {
                        hint_r = rr; hint_c = cc; hint_v = solved.cells[rr][cc];
                        break;
                    }
                }
                
                if (hint_r != -1) {
                    room_ptr->player_boards[pidx].cells[hint_r][hint_c] = hint_v;
                    pthread_mutex_unlock(&room_ptr->lock);
                    send_to_conn(room_ptr, sess->conn_id, "{\"type\":\"hint_ok\",\"row\":%d,\"col\":%d,\"value\":%d}", hint_r, hint_c, hint_v);
                    return;
                } else {
                    pthread_mutex_unlock(&room_ptr->lock);
                    send_to_conn(room_ptr, sess->conn_id, "{\"type\":\"error_msg\",\"message\":\"No empty cells! Click Check to see if you have mistakes.\"}");
                    return;
                }
            } else {
                pthread_mutex_unlock(&room_ptr->lock);
                send_to_conn(room_ptr, sess->conn_id, "{\"type\":\"error_msg\",\"message\":\"Board calculation error. Cannot generate hint.\"}");
                return;
            }
        }
        pthread_mutex_unlock(&room_ptr->lock);

    } else if (strcmp(type,"check_board")==0) {
        if (!sess->room || sess->room->status != 1) return;
        int pidx = -1;
        Room *room_ptr = sess->room;
        pthread_mutex_lock(&room_ptr->lock);
        for (int i = 0; i < room_ptr->player_count; i++) {
            if (strcmp(room_ptr->player_names[i], sess->username) == 0) { pidx = i; break; }
        }
        if (pidx >= 0 && room_ptr->player_board_ready[pidx]) {
            Board clean = room_ptr->player_boards[pidx];
            for(int rr=0; rr<clean.size; rr++) for(int cc=0; cc<clean.size; cc++) if(!clean.given[rr][cc]) clean.cells[rr][cc] = 0;
            Board solved; memset(&solved, 0, sizeof(Board));
            if (solver_run_as_child(&clean, &solved) == 0 && solved.size > 0) {
                char errors[1024] = "["; 
                int err_count = 0;
                int empty_count = 0; 
                
                for (int rr=0; rr<solved.size; rr++) {
                    for (int cc=0; cc<solved.size; cc++) {
                        if (room_ptr->player_boards[pidx].cells[rr][cc] == 0) {
                            empty_count++;
                        } else if (room_ptr->player_boards[pidx].cells[rr][cc] != solved.cells[rr][cc]) {
                            char temp[32]; snprintf(temp, 32, "%s{\"r\":%d,\"c\":%d}", err_count==0?"":",", rr, cc);
                            strcat(errors, temp); err_count++;
                        }
                    }
                }
                strcat(errors, "]");
                pthread_mutex_unlock(&room_ptr->lock);
                
                if (err_count == 0 && empty_count == 0) {
                    long el = (long)(time(NULL) - room_ptr->start_time);
                    room_ptr->player_status[pidx] = 1;
                    room_ptr->player_finish_time[pidx] = time(NULL);
                    send_to_conn(room_ptr, sess->conn_id, "{\"type\":\"board_solved\",\"time\":%ld}", el);
                    broadcast_leaderboard(room_ptr);
                    
                    int all_done = 1;
                    for(int k=0; k<room_ptr->player_count; k++) {
                        if (room_ptr->player_status[k] == 0) { all_done = 0; break; }
                    }
                    if (all_done) {
                        room_ptr->status = 2;
                        broadcast_room(room_ptr, "{\"type\":\"game_over\"}");
                    }
                } else if (err_count == 0 && empty_count > 0) {
                    send_to_conn(room_ptr, sess->conn_id, "{\"type\":\"check_result\",\"errors\":[]}");
                } else {
                    send_to_conn(room_ptr, sess->conn_id, "{\"type\":\"check_result\",\"errors\":%s}", errors);
                }
            } else {
                pthread_mutex_unlock(&room_ptr->lock);
            }
        }
  } else if (strcmp(type,"auto_solve")==0) {
      if (!sess->room) return;
      Board clean = sess->room->board;
      for(int r=0; r<clean.size; r++) for(int c=0; c<clean.size; c++) if(!clean.given[r][c]) clean.cells[r][c] = 0;
      Board solved; memset(&solved, 0, sizeof(Board));
      int _dbg = solver_run_as_child(&clean, &solved);
      
      if (_dbg == 0 && solved.size > 0) {
        pthread_mutex_lock(&sess->room->lock);
        int pidx = -1;
        for (int i = 0; i < sess->room->player_count; i++) {
            if (strcmp(sess->room->player_names[i], sess->username) == 0) { pidx = i; break; }
        }
        if (pidx >= 0) {
            for (int r=0;r<solved.size;r++) {
                for (int c=0;c<solved.size;c++) {
                    if (!sess->room->player_boards[pidx].given[r][c]) 
                        sess->room->player_boards[pidx].cells[r][c] = solved.cells[r][c];
                }
            }
        }
        
        long elapsed = (long)(time(NULL) - sess->room->start_time);
        sess->room->player_status[pidx] = 1;
        sess->room->player_finish_time[pidx] = time(NULL);
        pthread_mutex_unlock(&sess->room->lock);

        char bj[8192]; board_to_json(&sess->room->player_boards[pidx],bj,sizeof(bj));
        send_to_conn(sess->room, sess->conn_id, "{\"type\":\"board_update\",\"board\":%s}",bj);
        send_to_conn(sess->room, sess->conn_id, "{\"type\":\"board_solved\",\"time\":%ld}", elapsed);
        broadcast_leaderboard(sess->room);
        
        int all_done = 1;
        for(int k=0; k<sess->room->player_count; k++) {
            if (sess->room->player_status[k] == 0) { all_done = 0; break; }
        }
        if (all_done) {
            sess->room->status = 2;
            broadcast_room(sess->room, "{\"type\":\"game_over\"}");
        }
      }
    } else if (strcmp(type,"leave_room")==0) {
        if (sess->room) {
            Room *r = sess->room;
            pthread_mutex_lock(&r->lock);
            r->player_count--;
            if (r->player_count <= 0) r->player_count = 0;
            pthread_mutex_unlock(&r->lock);
            sess->room = NULL;
            ws_queue(wsi, sess, "{\"type\":\"leave_ok\"}");
        }
    } else if (strcmp(type,"gallery_list")==0) {
        if (g_preset_count == 0) {
            Room dummy; pthread_mutex_init(&dummy.lock, NULL);
            load_board(&dummy, 1);
            pthread_mutex_destroy(&dummy.lock);
        }
        /* INCREASED: 64KB Temporary buffer for massive puzzle arrays */
        char pbuf[65000] = "[";
        for (int i = 0; i < g_preset_count; i++) {
            char bj[4096]; board_to_json(&g_presets[i], bj, sizeof(bj));
            
            /* WARNING FIX: Ensure target buffer 't' is larger than source 'bj' */
            char t[4600]; 
            snprintf(t, sizeof(t), "{\"id\":%d,\"title\":\"%s\",\"size\":%d,\"board\":%s}%s",
                     i+1, g_presets[i].title[0] ? g_presets[i].title : "Untitled", g_presets[i].size, bj, (i==g_preset_count-1)?"":",");
            strcat(pbuf, t);
        }
        strcat(pbuf, "]");
        ws_queue(wsi, sess, "{\"type\":\"gallery_data\",\"puzzles\":%s}", pbuf);
    }
}

static int callback_ws(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    WsSession *sess = (WsSession *)user;
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            memset(sess, 0, sizeof(WsSession));
            sess->user_id = -1;
            sess->conn_id = ++g_conn_counter;
            break;
        case LWS_CALLBACK_RECEIVE: {
            int rem = (int)(sizeof(sess->pending) - sess->pending_len - 1);
            int copy = (int)len < rem ? (int)len : rem;
            memcpy(sess->pending + sess->pending_len, in, copy);
            sess->pending_len += copy;
            sess->pending[sess->pending_len] = '\0';
            if (lws_is_final_fragment(wsi)) {
                handle_msg(wsi, sess, sess->pending);
                sess->pending_len = 0;
                sess->pending[0] = '\0';
            }
            break;
        }
        case LWS_CALLBACK_SERVER_WRITEABLE:
            if (sess->out_ready && sess->out_len > 0) {
                unsigned char *buf = malloc(LWS_PRE + sess->out_len + 1);
                if (buf) {
                    memcpy(buf + LWS_PRE, sess->out_buf, sess->out_len);
                    lws_write(wsi, buf + LWS_PRE, sess->out_len, LWS_WRITE_TEXT);
                    free(buf);
                }
                sess->out_ready = 0;
                sess->out_len = 0;
            }
            break;
        case LWS_CALLBACK_CLOSED:
            rs_remove(wsi);
            if (sess && sess->room) {
                Room *r = sess->room;
                pthread_mutex_lock(&r->lock);
                r->player_count--;
                if (r->player_count <= 0) r->player_count = 0;
                pthread_mutex_unlock(&r->lock);
                sess->room = NULL;
            }
            break;
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        .name = "ripple-protocol",
        .callback = callback_ws,
        .per_session_data_size = sizeof(WsSession),
        .rx_buffer_size = 65536
    },
    { 0 }
};

static struct lws_context *g_lws_ctx = NULL;
static pthread_t g_lws_thread;
static pthread_t g_ldr_thread;

static void *lws_loop(void *arg) {
    (void)arg;
    while (g_running) lws_service(g_lws_ctx, 50);
    return NULL;
}

void websocket_start(int port) {
    srand(time(NULL));
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = protocols;
    info.mounts = NULL;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8 | LWS_SERVER_OPTION_SKIP_SERVER_CANONICAL_NAME;
    g_lws_ctx = lws_create_context(&info);
    if (!g_lws_ctx) return;
    pthread_create(&g_lws_thread, NULL, lws_loop, NULL);
    pthread_create(&g_ldr_thread, NULL, leaderboard_thread, NULL);
}

void websocket_stop(void) {
    if (g_lws_ctx) {
        pthread_join(g_lws_thread, NULL);
        pthread_join(g_ldr_thread, NULL);
        lws_context_destroy(g_lws_ctx);
        g_lws_ctx = NULL;
    }
}
void websocket_service(void) {}
