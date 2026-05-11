#ifndef TUI_H
#define TUI_H
#include <ncurses.h>
void tui_init(void);
void tui_cleanup(void);
void tui_run(void);
void tui_draw_rooms(WINDOW *win);
void tui_draw_leaderboard(WINDOW *win);
void tui_draw_gallery(WINDOW *win);
void tui_draw_stats(WINDOW *win);
void tui_handle_input(int ch);
void tui_kick_player(const char *party_code, int player_id);
void tui_approve_puzzle(int puzzle_id);
void tui_delete_puzzle(int puzzle_id);
#endif
