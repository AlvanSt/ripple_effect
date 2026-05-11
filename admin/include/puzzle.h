#ifndef PUZZLE_H
#define PUZZLE_H
#include "ripple.h"
int puzzle_load_file(const char *path, Board *boards, int max_count);
int puzzle_save_file(const char *path, Board *b);
int puzzle_validate(Board *b);
int puzzle_editor_run(Board *b);
#endif
