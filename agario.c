#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include "geometry.h"

#define MAX_EVENTS 5
#define MAX_PLAYERS 64
#define FIELD_HEIGHT 1000
#define FIELD_WIDTH 1000

static char *too_many_connections_message = "too_many_connections";

typedef struct player {
    int sock;
    int id;
    int size;
    vec2_t pos;
    vec2_t target;
} player_t;

typedef struct context {
    int epoll_fd;
    player_t *players[MAX_PLAYERS];
} context_t;

static vec2_t generate_player_pos() {
    float x = rand() % FIELD_WIDTH;
    float y = rand() % FIELD_HEIGHT;
    return (vec2_t){x, y};
}

static player_t *player_new(int sock, int id) {
    player_t *p = malloc(sizeof(player_t));
    p->sock = sock;
    p->id = id;
    p->size = 100;
    p->pos = generate_player_pos();
    p->target = p->pos;
    return p;
}

static int send_all(int sock, char *msg, int msg_len) {
    int sent;
    while (msg_len > 0) {
        sent = send(sock, msg, msg_len, 0);
        if (sent == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        msg += sent;
        msg_len -= sent;
    }
    return 0;
}

static void disconnect_player(player_t *player, context_t *ctx) {
    int idx = 0;
    while (idx < MAX_PLAYERS && ctx->players[idx] != player) {
        idx++;
    }

    if (idx < MAX_PLAYERS) {
        epoll_ctl(ctx->epoll_fd, EPOLL_CTL_DEL, player->sock, NULL);
        close(player->sock);
        free(player);
        ctx->players[idx] = NULL;
    } else {
        printf("tried to disconnect player that wasn't in player list\n");
    }
}

static int join_player(int sock, context_t *ctx) {
    struct epoll_event event = {};
    int ret, idx = 0;

    while (idx < MAX_PLAYERS && ctx->players[idx]) {
        idx++;
    }

    if (idx == MAX_PLAYERS) {
        printf("Got too many connections");
        // TODO: Swap for binary error message once protocol is established
        send_all(sock, too_many_connections_message, strlen(too_many_connections_message));
        close(sock);
        return -1;
    }

    player_t *player = player_new(sock, idx);

    event.events = EPOLLIN;
    event.data.ptr = player;
    ret = epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, sock, &event);
    if (ret == -1) {
        printf("Error adding player socket to epoll instance, closing socket\n");
        free(player);
        close(sock);
    } else {
        ctx->players[idx] = player;
        // TODO: Send position to player
    }

    return ret;
}

static void broadcast(char *msg, int msg_len, context_t *ctx) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        player_t *player = ctx->players[i];
        if (player) {
            if (send_all(player->sock, msg, msg_len) == -1) {
                printf("Error when sending to socket: errno %d -- %s\n", errno, strerror(errno));
                printf("Closing socket\n");
                disconnect_player(player, ctx);
            }
        }
    }
}

int main(void) {
    int server_sock, client_sock, epoll_fd, running = 1, event_count, i;
    struct sockaddr_in server_addr;
    char client_message[2000] = {};
    ssize_t bytes_received;
    struct epoll_event event = {}, events[MAX_EVENTS] = {};
    context_t ctx = {};

    // Ignore SIGPIPE signal, which would cause a process exit when we try to
    // send or receive on a broken stream
    signal(SIGPIPE, SIG_IGN);

    // Seed random generator
    srand(time(NULL));

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        printf("Error while creating socket\n");
        return 1;
    }
    printf("Socket created successfully\n");

    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        // Non-fatal error, continue execution
        printf("Setting REUSEADDR failed on socket: errno %d -- %s\n", errno, strerror(errno));
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        printf("Couldn't bind to the port: errno %d -- %s\n", errno, strerror(errno));
        close(server_sock);
        return 1;
    }
    printf("Binded the socket\n");

    if (listen(server_sock, 0) == -1) {
        printf("Error while listening\n");
        close(server_sock);
        return 1;
    }
    printf("Listening for incoming connections...\n");

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        printf("Error creating epoll instance\n");
        close(server_sock);
        return 1;
    }
    ctx.epoll_fd = epoll_fd;

    event.events = EPOLLIN;
    event.data.ptr = NULL;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sock, &event)) {
        printf("Error adding server_sock to epoll instance\n");
        close(server_sock);
        close(epoll_fd);
        return 1;
    }

    while (running) {
        event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (i = 0; i < event_count; i++) {
            // The server socket doesn't have data associated with it in epoll
            if (!events[i].data.ptr) {
                client_sock = accept(server_sock, NULL, NULL);
                printf("got client_sock: %d\n", client_sock);
                if (client_sock < 0) {
                    printf("Couldn't accept connection...\n");
                    continue;
                }
                if (join_player(client_sock, &ctx) == -1) {
                    continue;
                }
            } else {
                player_t *player = events[i].data.ptr;
                printf("player socket ready: %d\n", player->sock);
                bytes_received = recv(player->sock, client_message, sizeof(client_message), 0);
                // Be careful when handling errno, because calls to printf can overwrite it
                if (bytes_received == -1 && errno != EINTR) {
                    printf("Error when receiving from socket: errno %d -- %s\n", errno, strerror(errno));
                    printf("Closing socket\n");
                    disconnect_player(player, &ctx);
                    continue;
                }
                // Closed orderly by peer
                if (bytes_received == 0) {
                    disconnect_player(player, &ctx);
                    continue;
                }

                broadcast(client_message, bytes_received, &ctx);
            }
        }
    }

    // TODO: Close all client sockets (first check if that is required)
    close(server_sock);
    close(epoll_fd);

    return 0;
}
