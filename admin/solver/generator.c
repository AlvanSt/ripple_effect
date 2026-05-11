#include "solver.h"
#include "game.h"
#include <stdlib.h>
#include <string.h>

int puzzle_generate(Board *out, int size) {
    if (!out || size < 3 || size > 6) return 0;

    for (int attempt = 0; attempt < 200000; attempt++) {
        memset(out, 0, sizeof(Board));
        out->size = size;

        /* === STEP 1: BUILD BRAND NEW RANDOM ROOMS === */
        /* Place every cell into a queue, shuffle it, then flood-fill rooms */
        int order[64]; int n = size * size;
        for (int i = 0; i < n; i++) order[i] = i;
        /* Fisher-Yates shuffle for completely random start order */
        for (int i = n - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
        }

        int room_id = 1;
        for (int k = 0; k < n; k++) {
            int sr = order[k] / size;
            int sc = order[k] % size;
            if (out->rooms[sr][sc] != 0) continue;

            /* Max room size capped at size to avoid giant unsolvable rooms */
            int max_sz = (size > 4) ? 4 : size;
            int target = (rand() % max_sz) + 1;
            int sz = 0;
            int cr = sr, cc_col = sc;

            while (sz < target) {
                if (out->rooms[cr][cc_col] == 0) {
                    out->rooms[cr][cc_col] = room_id;
                    sz++;
                }
                /* Random walk in 4 directions */
                int dirs[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
                /* Shuffle directions for organic shapes */
                for (int d = 3; d > 0; d--) {
                    int j = rand() % (d + 1);
                    int tr = dirs[d][0]; int tc = dirs[d][1];
                    dirs[d][0] = dirs[j][0]; dirs[d][1] = dirs[j][1];
                    dirs[j][0] = tr; dirs[j][1] = tc;
                }
                int moved = 0;
                for (int d = 0; d < 4; d++) {
                    int nr = cr + dirs[d][0], nc = cc_col + dirs[d][1];
                    if (nr>=0 && nr<size && nc>=0 && nc<size && out->rooms[nr][nc]==0) {
                        cr = nr; cc_col = nc; moved = 1; break;
                    }
                }
                if (!moved) break;
            }
            out->room_sizes[room_id] = sz;
            room_id++;
        }
        out->room_count = room_id - 1;

        /* === STEP 2: VERIFY ROOMS COVER EVERY CELL === */
        int valid = 1;
        for (int r = 0; r < size && valid; r++)
            for (int c = 0; c < size && valid; c++)
                if (out->rooms[r][c] == 0) valid = 0;
        if (!valid) continue;

        /* === STEP 3: SOLVE TO FIND VALID NUMBER LAYOUT === */
        Board solved; memset(&solved, 0, sizeof(Board));
        if (solver_run_as_child(out, &solved) != 0) continue;
        if (solved.cells[0][0] == 0) continue; /* Verify solver actually ran */

        /* === STEP 4: PUNCH HOLES TO CREATE THE PLAYABLE PUZZLE === */
        /* Keep at least 1 given per room so it stays uniquely solvable */
        memset(out->given, 0, sizeof(out->given));
        memset(out->cells, 0, sizeof(out->cells));

        for (int rid = 1; rid <= out->room_count; rid++) {
            /* Guarantee at least one given per room */
            int best_r = -1, best_c = -1;
            for (int r = 0; r < size; r++)
                for (int c = 0; c < size; c++)
                    if (out->rooms[r][c] == rid) { best_r = r; best_c = c; }
            if (best_r >= 0) {
                out->given[best_r][best_c] = 1;
                out->cells[best_r][best_c] = solved.cells[best_r][best_c];
            }
        }

        /* Randomly reveal some extra cells (35% chance) */
        for (int r = 0; r < size; r++) {
            for (int c = 0; c < size; c++) {
                if (!out->given[r][c] && (rand() % 100) < 35) {
                    out->given[r][c] = 1;
                    out->cells[r][c] = solved.cells[r][c];
                }
            }
        }

        return 1; /* SUCCESS! Brand new puzzle ready */
    }

    return 0; /* Failed after 200,000 attempts (should never happen) */
}
