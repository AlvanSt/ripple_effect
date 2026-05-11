#include "puzzle.h"
#include <ncurses.h>
#include <string.h>

int puzzle_editor_run(Board *b) {
    int cur_r = 0, cur_c = 0;
    while (1) {
        clear();
        mvprintw(0, 0, "Puzzle Editor — %dx%d  [arrow=move] [1-%d=value] [s=save] [q=quit]",
                 b->size, b->size, b->size);
        for (int r = 0; r < b->size; r++) {
            for (int c = 0; c < b->size; c++) {
                if (r == cur_r && c == cur_c) attron(A_REVERSE);
                mvprintw(r + 2, c * 3, "%2d", b->cells[r][c]);
                if (r == cur_r && c == cur_c) attroff(A_REVERSE);
            }
        }
        refresh();
        int ch = getch();
        switch (ch) {
            case KEY_UP:    cur_r = (cur_r - 1 + b->size) % b->size; break;
            case KEY_DOWN:  cur_r = (cur_r + 1)           % b->size; break;
            case KEY_LEFT:  cur_c = (cur_c - 1 + b->size) % b->size; break;
            case KEY_RIGHT: cur_c = (cur_c + 1)           % b->size; break;
            case 's': case 'S':
                if (puzzle_validate(b)) {
                    mvprintw(b->size + 3, 0, "Valid! Saving...");
                    refresh();
                    return 0;
                } else {
                    mvprintw(b->size + 3, 0, "Not solvable — keep editing.");
                    refresh();
                }
                break;
            case 'q': case 'Q': return -1;
            default:
                if (ch >= '1' && ch <= '0' + b->size) {
                    b->cells[cur_r][cur_c] = ch - '0';
                    b->given[cur_r][cur_c] = 1;
                } else if (ch == '0' || ch == KEY_DC) {
                    b->cells[cur_r][cur_c] = 0;
                    b->given[cur_r][cur_c] = 0;
                }
        }
    }
}
