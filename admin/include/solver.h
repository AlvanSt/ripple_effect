#ifndef SOLVER_H
#define SOLVER_H
#include "ripple.h"
int solver_solve(Board *b);
int solver_generate(Board *b, int size, int difficulty);
int solver_run_as_child(Board *b, Board *out_solved);
int puzzle_generate(Board *out_board, int size);
#endif
