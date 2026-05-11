#include "puzzle.h"
#include "solver.h"

int puzzle_validate(Board *b) {
    Board copy = *b;
    return solver_solve(&copy);
}
