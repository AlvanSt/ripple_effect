CC      = gcc
CFLAGS  = -Wall -Wextra -g -pthread \
          -I admin/include \
          -I /usr/include \
          -I /usr/local/include

LDFLAGS = -lncurses \
          -lwebsockets \
          -L/usr/lib \
          -L/usr/lib/x86_64-linux-gnu \
          -L/usr/local/lib \
          -lsqlite3 \
          -lcrypto \
          -lm

SRCS = admin/src/main.c \
       admin/src/game.c \
       admin/src/board.c \
       admin/src/server.c \
       admin/src/websocket.c \
       admin/src/auth.c \
       admin/src/session.c \
       admin/src/signals.c \
       admin/src/threads.c \
       admin/src/epoll_io.c \
       admin/tui/tui.c \
       admin/tui/tui_rooms.c \
       admin/tui/tui_gallery.c \
       admin/db/database.c \
       admin/db/schema.c \
       admin/solver/solver.c \
       admin/solver/generator.c \
       admin/puzzle/loader.c \
       admin/puzzle/editor.c \
       admin/puzzle/validator.c

TARGET = ripple_server

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) *.log *.db

.PHONY: all clean
