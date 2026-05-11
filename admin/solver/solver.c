#include "game.h"
#include "solver.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

static int is_valid(Board *b, int r, int c, int v) {
    int rid = b->rooms[r][c];
    if (v > b->room_sizes[rid]) return 0;
    for (int i=0; i<b->size; i++)
        for (int j=0; j<b->size; j++)
            if (b->rooms[i][j]==rid && (i!=r||j!=c) && b->cells[i][j]==v) return 0;
    for (int cc=0; cc<b->size; cc++)
        if (cc!=c && b->cells[r][cc]==v && abs(c-cc)<=v) return 0;
    for (int rr=0; rr<b->size; rr++)
        if (rr!=r && b->cells[rr][c]==v && abs(r-rr)<=v) return 0;
    return 1;
}

static int solve_rec(Board *b) {
    int br=-1, bc=-1, mm=999, ef=0;
    for (int r=0; r<b->size; r++) {
        for (int c=0; c<b->size; c++) {
            if (b->cells[r][c]==0) {
                ef=1;
                int mv=0;
                for (int v=1; v<=b->room_sizes[b->rooms[r][c]]; v++)
                    if (is_valid(b,r,c,v)) mv++;
                if (mv==0) return 0;
                if (mv<mm) { mm=mv; br=r; bc=c; if (mv==1) goto pick; }
            }
        }
    }
pick:
    if (!ef) return 1;
    for (int v=1; v<=b->room_sizes[b->rooms[br][bc]]; v++) {
        if (is_valid(b,br,bc,v)) {
            b->cells[br][bc]=v;
            if (solve_rec(b)) return 1;
            b->cells[br][bc]=0;
        }
    }
    return 0;
}

int solver_solve(Board *b) { return solve_rec(b); }

typedef struct { Board in; Board out; int ok; } SolveJob;
static void *tfn(void *a) {
    SolveJob *j=(SolveJob*)a;
    j->out=j->in; j->ok=solve_rec(&j->out); return NULL;
}

int solver_run_as_child(Board *b, Board *out) {
    SolveJob j; j.in=*b; j.ok=0;
    pthread_t tid;
    if (pthread_create(&tid,NULL,tfn,&j)!=0) return -1;
    pthread_join(tid,NULL);
    if (j.ok) { *out=j.out; return 0; }
    return -1;
}
