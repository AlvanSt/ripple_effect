#include "tui.h"
#include "server.h"
#include "game.h"
#include "database.h"
#include "puzzle.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

extern Room g_rooms[];
extern int g_room_count;
extern Board g_presets[];
extern int g_preset_count;

void tui_view_rooms(void);
void tui_view_gallery(void);
void tui_view_users(void);
void tui_view_board(const char *party_code);

void tui_init(void) {
    initscr(); cbreak(); noecho();
    keypad(stdscr, TRUE); curs_set(0);
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
        init_pair(4, COLOR_BLUE, COLOR_BLACK);
    }
}

void tui_cleanup(void) { endwin(); }

static void draw_menu(int max_y, int max_x) {
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(2, (max_x - 30) / 2, "🌊 RIPPLE EFFECT - ADMIN CONSOLE 🌊");
    attroff(COLOR_PAIR(1) | A_BOLD);
    mvprintw(5, 4, "[R] View Active Rooms (%d)", g_room_count);
    mvprintw(6, 4, "[P] Puzzle Gallery Management");
    mvprintw(7, 4, "[U] User Management");
    mvprintw(8, 4, "[B] Live Board Viewer");
    mvprintw(9, 4, "[Q] Quit Server");
    attron(COLOR_PAIR(2));
    mvprintw(max_y - 2, 2, "Status: ONLINE | TCP:8080 | WS:8081");
    attroff(COLOR_PAIR(2));
}

void tui_run(void) {
    int max_y, max_x;
    while (g_running) {
        clear();
        getmaxyx(stdscr, max_y, max_x);
        draw_menu(max_y, max_x);
        refresh();

        int ch = getch();
        switch (ch) {
            case 'q': case 'Q': g_running = 0; break;
            case 'r': case 'R': tui_view_rooms(); break;
            case 'p': case 'P': tui_view_gallery(); break;
            case 'u': case 'U': tui_view_users(); break;
            case 'b': case 'B': {
                echo(); curs_set(1);
                char code[32] = {0};
                mvprintw(max_y - 1, 0, "Enter Party Code to view: ");
                getnstr(code, 6);
                noecho(); curs_set(0);
                if (strlen(code) > 0) tui_view_board(code);
                break;
            }
        }
    }
}

void tui_view_rooms(void) {
    int rows, cols; getmaxyx(stdscr, rows, cols);
    WINDOW *win = newwin(rows, cols, 0, 0);
    keypad(win, TRUE); nodelay(win, TRUE);
    while (g_running) {
        werase(win); box(win, 0, 0);
        mvwprintw(win, 1, 2, " ACTIVE ROOMS (%d) ", g_room_count);
        mvwprintw(win, 3, 2, "%-10s | %-10s | %-10s", "Room Code", "Players", "Status");
        mvwprintw(win, 4, 2, "----------------------------------------");
        for (int i = 0; i < g_room_count && i < rows - 8; i++) {
            Room *r = &g_rooms[i];
            const char *st = (r->player_count == 0 && r->status != 0) ? "Leaved" : (r->status == 1 ? "Playing" : (r->status == 2 ? "Won" : "Waiting"));
            mvwprintw(win, 5 + i, 2, "%-10s | %-10d | %-10s", r->code, r->player_count, st);
        }
        mvwprintw(win, rows - 2, 2, " Press 'q' to go back ");
        wrefresh(win);
        struct timespec ts = {0, 200000000L}; nanosleep(&ts, NULL);
        int ch = wgetch(win);
        if (ch == 'q' || ch == 'Q' || ch == 27) break;
    }
    delwin(win); touchwin(stdscr); refresh();
}

void tui_view_gallery(void) {
    int rows, cols; getmaxyx(stdscr, rows, cols);
    WINDOW *win = newwin(rows, cols, 0, 0);
    werase(win); box(win, 0, 0);
    mvwprintw(win, 1, 2, " PUZZLE GALLERY (%d loaded) ", g_preset_count);
    mvwprintw(win, 3, 2, "%-5s | %-25s | %-5s", "ID", "Title", "Size");
    mvwprintw(win, 4, 2, "--------------------------------------------------");
    for (int i = 0; i < g_preset_count && i < rows - 8; i++) {
        mvwprintw(win, 5 + i, 2, "%-5d | %-25s | %dx%d",
                  g_presets[i].puzzle_id,
                  g_presets[i].title[0] ? g_presets[i].title : "Unknown",
                  g_presets[i].size, g_presets[i].size);
    }
    mvwprintw(win, rows - 2, 2, " Press any key to go back ");
    wrefresh(win); wgetch(win); delwin(win); touchwin(stdscr); refresh();
}

void tui_view_users(void) {
    int rows, cols; getmaxyx(stdscr, rows, cols);
    WINDOW *win = newwin(rows, cols, 0, 0);
    werase(win); box(win, 0, 0);
    mvwprintw(win, 1, 2, " USER MANAGEMENT ");
    mvwprintw(win, 3, 2, "Database File: ripple.db");
    mvwprintw(win, 5, 2, "To view all registered users, run this command in a separate terminal:");
    wattron(win, COLOR_PAIR(3));
    mvwprintw(win, 7, 4, "sqlite3 ripple.db 'SELECT id, username FROM users;'");
    wattroff(win, COLOR_PAIR(3));
    mvwprintw(win, rows - 2, 2, " Press any key to go back ");
    wrefresh(win); wgetch(win); delwin(win); touchwin(stdscr); refresh();
}

void tui_view_board(const char *party_code) {
    Room *room = game_find_room(party_code);
    if (!room) {
        mvprintw(LINES - 1, 0, "Room '%s' not found. Press any key.", party_code);
        clrtoeol(); refresh(); getch(); return;
    }

    int rows, cols; getmaxyx(stdscr, rows, cols);
    WINDOW *bwin = newwin(rows, cols / 2, 0, cols / 2);
    keypad(bwin, TRUE); nodelay(bwin, TRUE);

    while (g_running) {
        werase(bwin); box(bwin, 0, 0);
        mvwprintw(bwin, 1, 2, " Live View: Room %s ", party_code);
        mvwprintw(bwin, 2, 2, " Players: %d | Status: %s ",
                  room->player_count,
                  room->status == 1 ? "Playing" : (room->status == 2 ? "Won" : "Waiting"));

        int start_y = 4, start_x = 4;
        pthread_mutex_lock(&room->lock);
        
        // FIX: Look at the active player board, not the static template
        Board *b = (room->player_count > 0 && room->player_board_ready[0]) ? &room->player_boards[0] : &room->board;
        
        for (int r = 0; r < b->size; r++) {
            for (int c = 0; c < b->size; c++) {
                int cell_y = start_y + (r * 2), cell_x = start_x + (c * 4);
                if (b->cells[r][c] > 0) {
                    if (b->given[r][c]) wattron(bwin, COLOR_PAIR(3));
                    mvwprintw(bwin, cell_y, cell_x, " %d ", b->cells[r][c]);
                    if (b->given[r][c]) wattroff(bwin, COLOR_PAIR(3));
                } else mvwprintw(bwin, cell_y, cell_x, " . ");

                if (c < b->size - 1) {
                    if (b->rooms[r][c] != b->rooms[r][c+1]) {
                        wattron(bwin, COLOR_PAIR(4) | A_BOLD);
                        mvwprintw(bwin, cell_y, cell_x + 3, "|");
                        wattroff(bwin, COLOR_PAIR(4) | A_BOLD);
                    } else mvwprintw(bwin, cell_y, cell_x + 3, " ");
                }
                if (r < b->size - 1) {
                    if (b->rooms[r][c] != b->rooms[r+1][c]) {
                        wattron(bwin, COLOR_PAIR(4) | A_BOLD);
                        mvwprintw(bwin, cell_y + 1, cell_x, "---");
                        wattroff(bwin, COLOR_PAIR(4) | A_BOLD);
                    } else mvwprintw(bwin, cell_y + 1, cell_x, "   ");
                }
            }
        }
        pthread_mutex_unlock(&room->lock);

        mvwprintw(bwin, rows - 2, 2, " Press 'q' to exit ");
        wrefresh(bwin);
        struct timespec ts = {0, 100000000L}; nanosleep(&ts, NULL);
        int ch = wgetch(bwin);
        if (ch == 'q' || ch == 'Q' || ch == 27) break;
    }
    nodelay(bwin, FALSE); delwin(bwin); touchwin(stdscr); refresh();
}
