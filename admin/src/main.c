#include "ripple.h"
#include "database.h"
#include "server.h"
#include "tui.h"
#include "puzzle.h"
#include "game.h"
#include <getopt.h>
#include <signal.h>

volatile sig_atomic_t g_running    = 1;
Room                  g_rooms[MAX_ROOMS];
int                   g_room_count = 0;
pthread_mutex_t       g_rooms_lock = PTHREAD_MUTEX_INITIALIZER;

extern Board g_presets[];
extern int   g_preset_count;

void websocket_start(int port);
void websocket_stop(void);

static void handle_sigterm(int s) { (void)s; g_running = 0; }
static void handle_sigalrm(int s) {
    (void)s;
    fprintf(stderr, "[SIGALRM] Rest reminder\n");
    alarm(300);
}

int main(int argc, char *argv[]) {
    int  port = 8080;
    char db_path[256]     = "ripple.db";
    char puzzle_file[256] = "";

    static struct option long_opts[] = {
        {"port", required_argument, 0, 'p'},
        {"db",   required_argument, 0, 'd'},
        {"size", required_argument, 0, 's'},
        {0,0,0,0}
    };
    int opt, idx = 0;
    while ((opt = getopt_long(argc, argv, "p:d:s:f:", long_opts, &idx)) != -1) {
        switch (opt) {
            case 'p': port = atoi(optarg);               break;
            case 'd': strncpy(db_path,     optarg, 255); break;
            case 'f': strncpy(puzzle_file, optarg, 255); break;
            case 's': break;
        }
    }

    signal(SIGTERM, handle_sigterm);
    signal(SIGALRM, handle_sigalrm);
    signal(SIGPIPE, SIG_IGN);
    alarm(300);

    db_open(db_path);
    db_init_schema();

    if (puzzle_file[0]) {
        g_preset_count = puzzle_load_file(puzzle_file, g_presets, 16);
        fprintf(stdout, "[INFO] Loaded %d puzzle(s) from %s\n",
                g_preset_count, puzzle_file);
        for (int i = 0; i < g_preset_count; i++)
            fprintf(stdout, "  [%d] '%s' %dx%d\n",
                    i+1, g_presets[i].title,
                    g_presets[i].size, g_presets[i].size);
    }

    server_start(port);
    websocket_start(port + 1);
    fprintf(stdout, "[INFO] TCP=%d  WS=%d  DB=%s\n", port, port+1, db_path);

    tui_init();
    tui_run();
    tui_cleanup();

    websocket_stop();
    server_stop();
    db_close();
    return 0;
}
