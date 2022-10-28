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
#include <stdbool.h>

#include "geometry.h"
#include "protocol.h"

#define MAX_EVENTS 5
#define MAX_PLAYERS 64
// Has to be greater than 1
#define TICKS_PER_SEC 20
#define FIELD_HEIGHT 1000
#define FIELD_WIDTH 1000

#define START_MASS 10

static char *too_many_connections_message = "too_many_connections";

// TODO: Add way to disconnect players that don't join in time to prevent denial
// of service attacks
typedef struct player_t {
    int sock;
    int id;
    int mass;
    char *name;
    rejoin_token_t rejoin_token;
    vec2_t pos;
    vec2_t target;
    bool joined;
} player_t;

typedef struct context_t {
    int epoll_fd;
    int next_player_id;
    player_t *players[MAX_PLAYERS];
} context_t;

static vec2_t generate_player_pos() {
    float x = rand() % FIELD_WIDTH;
    float y = rand() % FIELD_HEIGHT;
    return (vec2_t){x, y};
}

static player_t *player_new(int sock) {
    player_t *p = calloc(1, sizeof(player_t));
    p->sock = sock;
    p->joined = false;
    return p;
}

static void player_free(player_t *p) {
    if (p->name) {
        free(p->name);
    }
    free(p);
}

static int generate_player_id(context_t *ctx) {
    return ctx->next_player_id++;
}

static int get_player_count(context_t *ctx) {
    int count = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (ctx->players[i] && ctx->players[i]->joined) {
            count++;
        }
    }
    return count;
}

static int send_all(int sock, uint8_t *msg, int msg_len) {
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

static int connect_player(int sock, context_t *ctx) {
    struct epoll_event event = {};
    int ret, idx = 0;

    while (idx < MAX_PLAYERS && ctx->players[idx]) {
        idx++;
    }

    if (idx == MAX_PLAYERS) {
        printf("player tried to connect while game was full\n");
        // TODO: Swap for binary error message once protocol is established
        send_all(sock, (uint8_t *)too_many_connections_message, strlen(too_many_connections_message));
        close(sock);
        return -1;
    }

    player_t *player = player_new(sock);

    event.events = EPOLLIN;
    event.data.ptr = player;
    ret = epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, sock, &event);
    if (ret == -1) {
        printf("Error adding player socket to epoll instance, closing socket\n");
        player_free(player);
        close(sock);
    } else {
        ctx->players[idx] = player;
    }

    return ret;
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

static void join_player(player_t *player, join_message_t *msg, context_t *ctx) {
    char *name = msg->name ? msg->name : DEFAULT_PLAYER_NAME;
    int name_len = msg->name ? msg->name_length : DEFAULT_PLAYER_NAME_LENGTH;

    player->id = generate_player_id(ctx);
    player->mass = START_MASS;
    player->name = malloc(name_len + 1);
    strcpy(player->name, name);
    // TODO: Generate rejoin token with cryptographic randomness
    memset(player->rejoin_token, 0, REJOIN_TOKEN_LEN);
    player->pos = generate_player_pos();
    player->target = player->pos;
    player->joined = true;
}

static void send_bytes(uint8_t *buf, int buf_len, player_t *player, context_t *ctx) {
    if (buf_len <= 0) {
        return;
    }

    if (send_all(player->sock, buf, buf_len) == -1) {
        printf("Error when sending to socket: errno %d -- %s\n", errno, strerror(errno));
        printf("Closing socket\n");
        // TODO: Handle disconnects in such a way, that there isn't the risk of
        // trying to send to a player that was disconnected (e.g. when sending
        // two messages to a player in during one tick).
        // This could be done by setting a flag on the player struct and
        // disconnecting players that have the flag set during each tick.
        // Sending a message to players that have that flag set should also be
        // skipped.
        disconnect_player(player, ctx);
    }
}

static void broadcast_bytes(uint8_t *buf, int buf_len, context_t *ctx) {
    if (buf_len <= 0) {
        return;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        player_t *player = ctx->players[i];
        if (player && player->joined) {
            send_bytes(buf, buf_len, player, ctx);
        }
    }
}

static void handle_player_message(uint8_t *recv_buf, uint16_t recv_len, player_t *player, context_t *ctx) {
    uint8_t send_buf[65535];
    int send_len;

    generic_message_t *generic_msg = deserialize_message(recv_buf, recv_len);
    if (generic_msg) {
        if (!player->joined && generic_msg->message_type == MSG_JOIN) {
            join_player(player, (join_message_t *)generic_msg, ctx);

            join_ack_message_t join_ack_msg = {};
            join_ack_msg.message_type = MSG_JOIN_ACK;
            join_ack_msg.player_id = player->id;
            memcpy(join_ack_msg.rejoin_token, player->rejoin_token, REJOIN_TOKEN_LEN);
            send_len = serialize_message(
                    (generic_message_t *)&join_ack_msg, send_buf, sizeof(send_buf));
            send_bytes(send_buf, send_len, player, ctx);

            player_join_message_t player_join_msg = {};
            player_join_msg.message_type = MSG_PLAYER_JOIN;
            player_join_msg.player_info.player_id = player->id;
            player_join_msg.player_info.name_length = strlen(player->name);
            player_join_msg.player_info.name = player->name;
            send_len = serialize_message(
                    (generic_message_t *)&player_join_msg, send_buf, sizeof(send_buf));
            broadcast_bytes(send_buf, send_len, ctx);

            current_players_message_t current_players_msg = {};
            current_players_msg.message_type = MSG_CURRENT_PLAYERS;
            int player_count = get_player_count(ctx);
            current_players_msg.player_count = player_count;
            current_players_msg.player_infos = calloc(player_count, sizeof(player_info_t));
            int info_idx = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (ctx->players[i] && ctx->players[i]->joined) {
                    current_players_msg.player_infos[info_idx].player_id = ctx->players[i]->id;
                    current_players_msg.player_infos[info_idx].name_length =
                        strlen(ctx->players[i]->name);
                    current_players_msg.player_infos[info_idx].name = ctx->players[i]->name;
                    info_idx++;
                }
            }
            send_len = serialize_message(
                    (generic_message_t *)&current_players_msg, send_buf, sizeof(send_buf));
            send_bytes(send_buf, send_len, player, ctx);
            free(current_players_msg.player_infos);
        } else if (player->joined) {
            switch (generic_msg->message_type) {
                // TODO: Handle other messages
            }
        }

        message_free(generic_msg);
    }
}

static void tick(context_t *ctx) {
    // TODO: Implement tick logic
}

int main(void) {
    int server_sock, client_sock, epoll_fd, timer_fd, running = 1, event_count, i;
    uint64_t tick_count;
    struct sockaddr_in server_addr;
    uint8_t client_message[2000] = {};
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
                if (connect_player(client_sock, &ctx) == -1) {
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
                // TODO: Buffer the data in a player-specific buffer or try to
                // peek the data and look if we can read enough data to get a
                // message.
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

                handle_player_message(client_message, bytes_received, player, &ctx);
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
