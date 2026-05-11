#include "ripple.h"
#include <string.h>

void board_init(Board *b, int size) {
    memset(b, 0, sizeof(*b));
    b->size = size;
}

void board_print(const Board *b) {
    for (int r = 0; r < b->size; r++) {
        for (int c = 0; c < b->size; c++)
            printf("%2d", b->cells[r][c]);
        printf("\n");
    }
}
