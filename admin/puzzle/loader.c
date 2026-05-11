#include "puzzle.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Maps any printable symbol → integer room ID */
typedef struct { char sym; int id; } SymMap;
#define MAX_SYMS 128

static int sym_to_id(SymMap *map, int *count, char sym) {
    for (int i = 0; i < *count; i++)
        if (map[i].sym == sym) return map[i].id;
    int id = ++(*count);
    map[*count - 1].sym = sym;
    map[*count - 1].id  = id;
    return id;
}

int puzzle_load_file(const char *path, Board *boards, int max_count) {
    FILE *fp = fopen(path, "r");
    if (!fp) { perror("puzzle_load_file"); return 0; }

    int   count   = 0;
    char  line[512];
    Board *b      = NULL;
    int   row     = 0;
    int   section = 0; /* 0=header 1=ROOMS 2=GRID */
    SymMap sym_map[MAX_SYMS];
    int    sym_count = 0;

    while (fgets(line, sizeof(line), fp) && count < max_count) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue; /* skip blank lines */

        if (strncmp(line, "SIZE", 4) == 0) {
            b = &boards[count];
            memset(b, 0, sizeof(*b));
            memset(sym_map, 0, sizeof(sym_map));
            sym_count = 0;
            b->size = atoi(line + 5);
            section = 0; row = 0;

        } else if (strncmp(line, "TITLE", 5) == 0) {
            strncpy(b->title, line + 6, 63);

        } else if (strcmp(line, "ROOMS") == 0) {
            section = 1; row = 0;

        } else if (strcmp(line, "GRID") == 0) {
            section = 2; row = 0;

        } else if (strcmp(line, "---") == 0) {
            if (b) {
                /* compute room sizes */
                memset(b->room_sizes, 0, sizeof(b->room_sizes));
                for (int r = 0; r < b->size; r++)
                    for (int c = 0; c < b->size; c++) {
                        int id = b->rooms[r][c];
                        if (id > 0 && id <= MAX_ROOMS_PER_BOARD) {
                            b->room_sizes[id]++;
                            if (id > b->room_count) b->room_count = id;
                        }
                    }
            }
            count++;
            b = NULL; section = 0; row = 0;

        } else if (b && row < b->size) {
            if (section == 1) {
                /* ROOMS: tokenise by whitespace, each token is a symbol (1 char) */
                char *tok = strtok(line, " \t");
                for (int c = 0; tok && c < b->size; c++, tok = strtok(NULL, " \t")) {
                    b->rooms[row][c] = sym_to_id(sym_map, &sym_count, tok[0]);
                }
                row++;
            } else if (section == 2) {
                /* GRID: '.' = 0 (empty), digit = given value */
                char *tok = strtok(line, " \t");
                for (int c = 0; tok && c < b->size; c++, tok = strtok(NULL, " \t")) {
                    if (tok[0] == '.') {
                        b->cells[row][c] = 0;
                        b->given[row][c] = 0;
                    } else {
                        b->cells[row][c] = atoi(tok);
                        b->given[row][c] = 1;
                    }
                }
                row++;
            }
        }
    }
    fclose(fp);
    return count;
}

int puzzle_save_file(const char *path, Board *b) {
    /* symbols for saving: A-Z a-z 0-9 @ # $ % ^ & * ! ? ~ */
    const char sym_table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
        "0123456789@#$%^&*!?~";
    FILE *fp = fopen(path, "a");
    if (!fp) return -1;
    fprintf(fp, "SIZE %d\nTITLE %s\nROOMS\n", b->size, b->title);
    for (int r = 0; r < b->size; r++) {
        for (int c = 0; c < b->size; c++) {
            int id = b->rooms[r][c] - 1; /* 1-based → 0-based index */
            char sym = (id >= 0 && id < (int)sizeof(sym_table) - 1)
                       ? sym_table[id] : '?';
            fprintf(fp, "%c%s", sym, c < b->size - 1 ? " " : "");
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "GRID\n");
    for (int r = 0; r < b->size; r++) {
        for (int c = 0; c < b->size; c++) {
            if (b->given[r][c])
                fprintf(fp, "%d%s", b->cells[r][c], c < b->size - 1 ? " " : "");
            else
                fprintf(fp, ".%s", c < b->size - 1 ? " " : "");
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "---\n");
    fclose(fp);
    return 0;
}
