#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/event.h>
#include <errno.h>
#include <sys/time.h>
#include <stdbool.h>

#include "geometry.h"
#include "protocol.h"
#include "networking.h"

#define MAX_EVENTS 5
#define MAX_PLAYERS 64
// Has to be greater than 1
#define TICKS_PER_SEC 20
#define FIELD_HEIGHT 1000
#define FIELD_WIDTH 1000

#define START_MASS 10

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
    int kq;
    int next_player_id;
    player_t *players[MAX_PLAYERS];
} context_t;

// TODO: Is there a way to handle this better? Putting it into a header would
// make it look like this is a public function.
static void broadcast_bytes(uint8_t *buf, int buf_len, context_t *ctx);

static vec2_t generate_player_pos(void) {
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

static int connect_player(int sock, context_t *ctx) {
    struct kevent change = {0};
    int ret, idx = 0;

    while (idx < MAX_PLAYERS && ctx->players[idx]) {
        idx++;
    }

    if (idx == MAX_PLAYERS) {
        printf("player tried to connect while game was full\n");

        join_error_message_t join_error_msg = {
            .message_type = MSG_JOIN_ERROR,
            .error_code = JOIN_ERR_GAME_FULL,
            .error_message_length = strlen(GAME_FULL_ERROR_MSG),
            .error_message = GAME_FULL_ERROR_MSG,
        };

        uint8_t send_buf[65535] = {0};
        int send_len = serialize_message((generic_message_t *)&join_error_msg, send_buf, sizeof(send_buf));
        if (send_len > 0) {
            send_all(sock, send_buf, send_len);
        }

        close(sock);
        return -1;
    }

    player_t *player = player_new(sock);

    EV_SET(&change, sock, EVFILT_READ, EV_ADD, 0, 0, player);
    ret = kevent(ctx->kq, &change, 1, NULL, 0, NULL);
    if (ret == -1) {
        perror("adding player to kqueue failed, closing socket");
        player_free(player);
        close(sock);
    } else {
        ctx->players[idx] = player;
    }

    return ret;
}

static void disconnect_player(player_t *player, context_t *ctx) {
    struct kevent change = {0};

    int idx = 0;
    while (idx < MAX_PLAYERS && ctx->players[idx] != player) {
        idx++;
    }

    if (idx < MAX_PLAYERS) {
        EV_SET(&change, player->sock, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        kevent(ctx->kq, &change, 1, NULL, 0, NULL);

        // This has to happen before broadcasting the player leave message,
        // otherwise an infinite loop is created, if sending to the player
        // fails.
        ctx->players[idx] = NULL;

        if (player->joined) {
            uint8_t send_buf[64] = {0};
            player_leave_message_t player_leave_msg = {
                .message_type = MSG_PLAYER_LEAVE,
                .player_id = player->id,
            };
            int send_len = serialize_message((generic_message_t *)&player_leave_msg, send_buf, sizeof(send_buf));
            broadcast_bytes(send_buf, send_len, ctx);
        }

        // TODO: Send message to client so it knows the disconnect isn't abnormal, but explicitly performed by server
        close(player->sock);

        player_free(player);
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
    generic_message_t *generic_msg = NULL;

    // TODO: Use returned len
    (void) deserialize_message(recv_buf, recv_len, &generic_msg);
    if (generic_msg) {
        if (!player->joined && generic_msg->message_type == MSG_JOIN) {
            join_player(player, (join_message_t *)generic_msg, ctx);

            join_ack_message_t join_ack_msg = {
                .message_type = MSG_JOIN_ACK,
                .player_id = player->id,
            };
            memcpy(join_ack_msg.rejoin_token, player->rejoin_token, REJOIN_TOKEN_LEN);
            send_len = serialize_message(
                    (generic_message_t *)&join_ack_msg, send_buf, sizeof(send_buf));
            send_bytes(send_buf, send_len, player, ctx);

            player_join_message_t player_join_msg = {
                .message_type = MSG_PLAYER_JOIN,
                .player_info = {
                    .player_id = player->id,
                    .name_length = strlen(player->name),
                    .name = player->name,
                },
            };
            send_len = serialize_message(
                    (generic_message_t *)&player_join_msg, send_buf, sizeof(send_buf));
            broadcast_bytes(send_buf, send_len, ctx);

            int player_count = get_player_count(ctx);
            current_players_message_t current_players_msg = {
                .message_type = MSG_CURRENT_PLAYERS,
                .player_count = player_count,
                .player_infos = calloc(player_count, sizeof(player_info_t)),
            };
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
                case MSG_LEAVE:
                {
                    disconnect_player(player, ctx);
                    break;
                }
                case MSG_SET_TARGET:
                {
                    set_target_message_t *msg = (set_target_message_t *)generic_msg;
                    if (msg->x < 0) msg->x = 0;
                    else if (msg->x > FIELD_WIDTH) msg->x = FIELD_WIDTH;
                    if (msg->y < 0) msg->y = 0;
                    else if (msg->y > FIELD_HEIGHT) msg->y = FIELD_HEIGHT;
                    player->target.x = msg->x;
                    player->target.y = msg->y;
                    break;
                }
            }
        }

        message_free(generic_msg);
    }
}

static void tick(context_t *ctx) {
    uint8_t send_buf[65535];
    int send_len;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        player_t *player = ctx->players[i];
        if (player) {
            vec2_t diff = vec2_sub(player->target, player->pos);
            if (vec2_abs(diff) > 1) {
                player->pos = vec2_add(player->pos, vec2_scale(vec2_norm(diff), 0.5));
            }
            printf("player %d: (%.1f, %.1f)\n", player->id, player->pos.x, player->pos.y);
        }
    }

    // TODO: Add eat logic

    int player_count = get_player_count(ctx);
    int player_idx = 0;
    player_positions_message_t player_pos_msg = {
        .message_type = MSG_PLAYER_POSITIONS,
        .player_count = player_count,
        .player_positions = calloc(player_count, sizeof(player_position_t)),
    };
    for (int i = 0; i < MAX_PLAYERS; i++) {
        player_t *player = ctx->players[i];
        if (player) {
            player_pos_msg.player_positions[player_idx].player_id = player->id;
            player_pos_msg.player_positions[player_idx].x = player->pos.x;
            player_pos_msg.player_positions[player_idx].y = player->pos.y;
            player_pos_msg.player_positions[player_idx].mass = player->mass;
            player_idx++;
        }
    }
    send_len = serialize_message((generic_message_t *)&player_pos_msg, send_buf, sizeof(send_buf));
    broadcast_bytes(send_buf, send_len, ctx);
    free(player_pos_msg.player_positions);
}

int main(void) {
    int server_sock, client_sock, kq, running = 1, event_count, i;
    struct sockaddr_in server_addr;
    uint8_t client_message[2000] = {0};
    ssize_t bytes_received;
    struct kevent change = {0}, events[MAX_EVENTS] = {0};
    struct timespec timeout = {0};
    long tick_nsec = 1e9 / TICKS_PER_SEC;
    context_t ctx = { .next_player_id = 1 };

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
    printf("Bound the socket\n");

    if (listen(server_sock, 0) == -1) {
        printf("Error while listening\n");
        close(server_sock);
        return 1;
    }
    printf("Listening for incoming connections...\n");

    kq = kqueue();
    if (kq == -1) {
        perror("creating kqueue failed");
        close(server_sock);
        return 1;
    }
    ctx.kq = kq;


    EV_SET(&change, server_sock, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(kq, &change, 1, NULL, 0, NULL) == -1) {
        perror("adding server socket to kqueue failed");
        close(server_sock);
        close(kq);
        return 1;
    }

    uintptr_t timer_ident = -1;
    EV_SET(&change, timer_ident, EVFILT_TIMER, EV_ADD, NOTE_NSECONDS | NOTE_CRITICAL, tick_nsec, NULL);
    if (kevent(kq, &change, 1, NULL, 0, NULL) == -1) {
        perror("adding timer event to kqueue failed");
        close(server_sock);
        close(kq);
        return 1;
    }

    while (running) {
        event_count = kevent(kq, NULL, 0, events, MAX_EVENTS, &timeout);
        if (event_count < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("waiting for kqueue events failed");
            return 1;
        }

        for (i = 0; i < event_count; i++) {
            if (events[i].filter == EVFILT_TIMER && events[i].ident == timer_ident) {
                for (int ticks = events[i].data; ticks > 0; ticks--) {
                    tick(&ctx);
                }
            } else if ((int)events[i].ident == server_sock) {
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
            } else {
                player_t *player = (player_t *)events[i].udata;
                printf("player socket ready: %d\n", player->sock);
                // TODO: First read the message header in a non-blocking way to
                // determine the message size (e.g. when only one byte is
                // available but we try reading two bytes, it would block)
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
    close(kq);

    return 0;
}
