# Ripple Effect – Real-Time Multi-Client Game Server

**Multiplayer logic-puzzle server written in C for Linux – CUHK Introduction to Systems Programming (full marks)**

![Language](https://img.shields.io/badge/Language-C-blue)
![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey)
![Concurrency](https://img.shields.io/badge/Concurrency-pthreads%20%7C%20epoll-green)
![Network](https://img.shields.io/badge/Network-TCP%20%7C%20WebSocket-orange)
![Storage](https://img.shields.io/badge/Storage-SQLite%20WAL-informational)

---

## Overview

One server process runs the whole game. Web players and an admin terminal connect at the same time and talk to the server through one shared JSON message protocol. Players join rooms with a party code, solve the same puzzle, and see a live leaderboard.

The server is **authoritative**: every move is checked on the server, so clients cannot cheat by sending invalid moves.

---

## Features

| Area | What it does |
|---|---|
| **Rooms** | Up to 64 rooms, 8 players per room, joined by party code |
| **Connections** | TCP listener and WebSocket listener (libwebsockets) on one host |
| **Leaderboard** | Dedicated thread broadcasts every player's status and elapsed time once per second |
| **Game logic** | Server-side rule checks and hints |
| **Solver** | Recursive backtracking with MRV heuristic, forced moves and early dead-end exit |
| **Generator** | Fisher–Yates shuffle and random-walk room growth (up to 200,000 attempts) |
| **Validator** | Rejects puzzles that have no solution |
| **Puzzle files** | Custom text format with `SIZE`, `TITLE`, `ROOMS` and `GRID` sections |
| **Accounts** | SHA-256 password hashing (OpenSSL), `/dev/urandom` session tokens, 24-hour expiry |
| **Admin TUI** | ncurses terminal UI with a live board viewer |
| **Puzzle editor** | Create and edit puzzles in the browser |
| **Storage** | SQLite in WAL mode |

---

## Architecture

```
      Web clients (browser)              Admin TUI (ncurses)
             │ WebSocket                        │ TCP
             ▼                                  ▼
  ┌──────────────────────────────────────────────────────┐
  │                 Single server process                │
  │                                                      │
  │   epoll accept loop ──► detached pthread per client  │
  │   leaderboard thread ──► broadcast every 1 second    │
  │                                                      │
  │   JSON protocol (hand-parsed in C)                   │
  │   Game logic: rules · hints · solver · generator     │
  │                                                      │
  │   Shared state, protected by:                        │
  │     global rooms mutex · per-room mutexes            │
  │     connection-table mutex · per-cell semaphores     │
  │                                                      │
  │   SQLite (WAL): accounts · sessions · puzzles        │
  └──────────────────────────────────────────────────────┘
```

### Concurrency

| Shared resource | Protection |
|---|---|
| Room list | Global rooms mutex |
| Each room | Per-room mutex |
| Connection table | Connection-table mutex |
| Each board cell | Per-cell semaphore |

- `epoll` drives the accept loop; each client is handled by a **detached pthread**.
- Per-connection state is allocated with `calloc` and freed when the client leaves. Rooms, players and boards are plain C structs.

### Messaging

- **One JSON protocol** for both TCP and WebSocket clients, parsed by hand in C (no JSON library).
- Each connection has its own **session buffer**. Queued WebSocket messages are **appended, not overwritten**, so no message is lost when several arrive close together.

### Signals

| Signal | Behaviour |
|---|---|
| `SIGTERM` | Graceful shutdown |
| `SIGALRM` | Rest reminder to players |
| `SIGPIPE` | Ignored, so a disconnected client cannot crash the server |

---

## Tech stack

| Area | Technology |
|---|---|
| Language | C |
| Concurrency | POSIX threads, mutexes, semaphores, `epoll` |
| Networking | TCP sockets, WebSocket (libwebsockets) |
| Security | OpenSSL SHA-256, `/dev/urandom` |
| Storage | SQLite (WAL) |
| Terminal UI | ncurses |
| CLI | `getopt_long` |
| Build | GCC, Makefile (`-Wall -Wextra -pthread`) |

---

## Getting started

### Install dependencies (Ubuntu / Debian)
```bash
sudo apt install build-essential libwebsockets-dev libssl-dev libsqlite3-dev libncurses-dev
```

### Build
```bash
git clone https://github.com/AlvanSt/ripple_effect.git
cd ripple_effect
make
```

### Run
```bash
./<server-binary> --help     # show all options
```
> Replace `<server-binary>` with the program name built by `make`.

1. Start the server.
2. Open the web client in a browser and connect.
3. Create a room and share the **party code** with up to 7 other players.
4. Start the admin TUI to watch every board live.
5. Stop with `Ctrl+C` or `kill -TERM <pid>` for a clean shutdown.

---

## Puzzle file format

| Section | Content |
|---|---|
| `SIZE` | Grid size |
| `TITLE` | Puzzle name |
| `ROOMS` | Room layout |
| `GRID` | Starting numbers |

Every puzzle is validated when loaded. Puzzles with no solution are rejected.

---

## Game rules

- The grid is divided into rooms. A room with *N* cells contains the numbers **1 to N**, each once.
- If the same number *k* appears twice in a row or column, there must be **at least *k* cells between them**.

---

## Author

**Alvan Szeto** – Information Engineering, The Chinese University of Hong Kong
Individual project – sole developer from design to delivery.
