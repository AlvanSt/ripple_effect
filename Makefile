CC = gcc
CFLAGS = -Wall -Wextra -Iadmin/include -g
LDFLAGS = -lsqlite3 -lwebsockets -lcrypto -lssl -lncurses -lpthread

SRCS = admin/src/main.c admin/src/auth.c admin/src/board.c admin/src/epoll_io.c \
       admin/src/game.c admin/src/server.c admin/src/signals.c \
       admin/src/threads.c admin/src/websocket.c \
       admin/db/database.c admin/db/schema.c \
       admin/puzzle/editor.c admin/puzzle/loader.c admin/puzzle/validator.c \
       admin/solver/generator.c admin/solver/solver.c \
       admin/tui/tui.c

OBJS = $(SRCS:.c=.o)
TARGET = ripple_server

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(OBJS) $(TARGET)
