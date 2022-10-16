#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include "geometry.h"
#include "protocol.h"

#define MAX_EVENTS 5
#define MAX_PLAYERS 64
// Has to be greater than 1
#define TICKS_PER_SEC 20
#define FIELD_HEIGHT 1000
#define FIELD_WIDTH 1000

static char *too_many_connections_message = "too_many_connections";

typedef struct player_t {
    int sock;
    int id;
    int mass;
    char *name;
    vec2_t pos;
    vec2_t target;
} player_t;

typedef struct context_t {
    int epoll_fd;
    player_t *players[MAX_PLAYERS];
} context_t;

static vec2_t generate_player_pos() {
    float x = rand() % FIELD_WIDTH;
    float y = rand() % FIELD_HEIGHT;
    return (vec2_t){x, y};
}

static player_t *player_new(int sock, int id, char *name) {
    size_t name_len;
    player_t *p = malloc(sizeof(player_t));
    p->sock = sock;
    p->id = id;
    p->mass = 10;
    if (name && (name_len = strlen(name)) <= MAX_PLAYER_NAME_LEN) {
        p->name = malloc(name_len + 1);
        strcpy(p->name, name);
    } else {
        p->name = malloc(strlen(DEFAULT_PLAYER_NAME) + 1);
        strcpy(p->name, DEFAULT_PLAYER_NAME);
    }
    p->pos = generate_player_pos();
    p->target = p->pos;
    return p;
}

static void player_free(player_t *p) {
    free(p->name);
    free(p);
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
        // TODO: Send message to client so it knows the disconnect isn't abnormal
        epoll_ctl(ctx->epoll_fd, EPOLL_CTL_DEL, player->sock, NULL);
        close(player->sock);
        player_free(player);
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

    // TODO: Use name provided by player
    player_t *player = player_new(sock, idx, NULL);

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

static void tick(context_t *ctx) {
    // TODO: Implement tick logic
}

int main(void) {
    int server_sock, client_sock, epoll_fd, timer_fd, running = 1, event_count, i;
    uint64_t tick_count;
    struct sockaddr_in server_addr;
    char client_message[2000] = {};
    ssize_t bytes_received;
    struct epoll_event event = {}, events[MAX_EVENTS] = {};
    struct itimerspec interval = {};
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

    timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timer_fd == -1) {
        printf("Error creating timerfd\n");
        close(server_sock);
        return 1;
    }

    interval.it_value.tv_sec = 0;
    interval.it_value.tv_nsec = 1e9 / TICKS_PER_SEC;
    interval.it_interval = interval.it_value;
    if (timerfd_settime(timer_fd, 0, &interval, NULL) == -1) {
        printf("Error configuring timerfd interval\n");
        close(server_sock);
        close(timer_fd);
        return 1;
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        printf("Error creating epoll instance\n");
        close(server_sock);
        close(timer_fd);
        return 1;
    }
    ctx.epoll_fd = epoll_fd;

    event.events = EPOLLIN;
    event.data.ptr = NULL;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sock, &event) == -1) {
        printf("Error adding server_sock to epoll instance\n");
        close(server_sock);
        close(timer_fd);
        close(epoll_fd);
        return 1;
    }

    event.events = EPOLLIN;
    event.data.u64 = 1;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &event) == -1) {
        printf("Error adding the timer_fd to epoll instance\n");
        close(server_sock);
        close(timer_fd);
        close(epoll_fd);
        return 1;
    }


    // TODO: Remove this later on
    printf("Testing protocol\n");

    uint8_t buf[65535] = {};
    join_message_t msg;
    msg.message_type = MSG_JOIN;
    msg.name_length = 5;
    msg.name = malloc(6);
    strncpy(msg.name, "Simon", 6);

    int msg_len = serialize_message((generic_message_t *)&msg, buf, 65535);
    printf("msg_len: %d\n", msg_len);
    for (int i = 0; i < msg_len; i++) {
        printf("%x ", buf[i]);
    }
    printf("\n");

    join_message_t *dmsg = (join_message_t *)deserialize_message(buf, msg_len);
    printf("message_type: %d\n", dmsg->message_type);
    printf("name_length: %d\n", dmsg->name_length);
    printf("name: %s\n", dmsg->name);
    message_free((generic_message_t *)dmsg);

    printf("Finished testing protocol\n");

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
                    // In case something gets added after the if statement later
                    continue;
                }
            } else if (events[i].data.u64 == 1) {
                read(timer_fd, &tick_count, 8);
                if (tick_count > 1) {
                    printf("warning: missed %ld ticks\n", tick_count - 1);
                }
                tick(&ctx);
                // TODO: Send updates to clients
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

    for (i = 0; i < MAX_PLAYERS; i++) {
        if (ctx.players[i]) {
            disconnect_player(ctx.players[i], &ctx);
        }
    }
    close(server_sock);
    close(timer_fd);
    close(epoll_fd);

    return 0;
}
