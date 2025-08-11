#include <stdlib.h>
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "raylib.h"

#include "tree.h"
#include "protocol.h"
#include "networking.h"
#include "raymath.h"

#define SEND_BUF_LEN 65535
#define RECV_BUF_LEN 65535

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 600

#define STATE_ENTER_NAME 1
#define STATE_JOINING 2
#define STATE_INGAME 3

#define TARGET_FPS 60

typedef struct player_state_t {
    uint32_t id;
    uint32_t mass;
    char *name;
    Vector2 pos;
} player_state_t;

player_state_t *player_state_new(uint32_t id, char *name) {
    player_state_t *player_state = calloc(1, sizeof(player_state_t));
    player_state->id = id;
    player_state->name = name;
    player_state->mass = -1;
    player_state->pos = (Vector2){-1, -1};
    return player_state;
}

void player_state_free(player_state_t *player_state) {
    if (player_state->name) {
        free(player_state->name);
    }
    free(player_state);
}

void player_state_free_void_ptr(void *player_state) {
    player_state_free((player_state_t *)player_state);
}

void clear_player_states(tree_t *player_states) {
    tree_clear(player_states, player_state_free_void_ptr);
}

void draw_fps(void) {
    char fps_str[8] = {0};
    int fps_font_size = 12;

    float frametime = GetFrameTime();
    int fps = 0;
    if (frametime) {
        fps = 1 / frametime;
    }
    snprintf(fps_str, sizeof(fps_str), "%d", fps);
    int fps_text_width = MeasureText(fps_str, fps_font_size);
    DrawText(fps_str, WINDOW_WIDTH - fps_text_width, 0, fps_font_size, WHITE);
}

// TODO: Remove these in favor of new for-each function for tree, that accepts context
uint32_t global_own_player_id;
float global_field_to_window_scale_factor;

void draw_player_pos(void *player_state_void_ptr) {
    player_state_t *player_state = player_state_void_ptr;

    Color color = player_state->id == global_own_player_id ? DARKBLUE : RED;
    Vector2 window_pos = Vector2Scale(player_state->pos, global_field_to_window_scale_factor);
    DrawCircleV(window_pos, 50, color);
}

int main(void) {
    struct sockaddr_in server_addr = {0};
    uint8_t send_buf[SEND_BUF_LEN] = {0};
    uint8_t recv_buf[RECV_BUF_LEN] = {0};

    int recv_buf_idx = 0;
    int n_bytes;

    int state = STATE_ENTER_NAME;
    int key, chr;

    rejoin_token_t rejoin_token = {0};
    uint32_t own_player_id = 0;
    // Start the position outside of the field
    Vector2 previous_display_target = {-1, -1};

    tree_t *player_states = tree_new();

    // TODO: Get the field size from server
    float field_to_window_scale_factor = (float)(WINDOW_WIDTH) / 1000.0;
    global_field_to_window_scale_factor = field_to_window_scale_factor;
    float window_to_field_scale_factor = 1 / field_to_window_scale_factor;

    SetTargetFPS(TARGET_FPS);

	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "AgarIO");

    BeginDrawing();
    ClearBackground(BLACK);
    DrawText("Connecting...", 0, 0, 24, WHITE);
    EndDrawing();

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Could not open socket");
        CloseWindow();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Could not connect to server");
        close(sock);
        return 1;
    }

    TraceLog(LOG_INFO, "Connected to server");

    if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
        perror("Could not put socket in non-blocking mode");
        close(sock);
        CloseWindow();
        return 1;
    }

    char player_name[MAX_PLAYER_NAME_LEN + 1] = {0};
    int player_name_chars = 0;

    int got_join_ack = 0;
    int got_current_players = 0;

	while (!WindowShouldClose())
	{
		BeginDrawing();

		ClearBackground(BLACK);


        // Networking
        {
            int space_in_buffer = RECV_BUF_LEN - recv_buf_idx;
            n_bytes = recv(sock, recv_buf + recv_buf_idx, space_in_buffer, 0);

            if (n_bytes == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    TraceLog(LOG_WARNING, "Error receiving from socket: %s", strerror(errno));
                }
            } else if (n_bytes == 0) {
                // TODO: Handle this better
                TraceLog(LOG_WARNING, "Server closed connection -- quitting");
                CloseWindow();
                return 1;
            } else {
                recv_buf_idx += n_bytes;

                generic_message_t *generic_msg = NULL;
                int msg_bytes = deserialize_message(recv_buf, recv_buf_idx, &generic_msg);

                if (generic_msg) {
                    assert(msg_bytes <= recv_buf_idx);

                    int bytes_left_in_buffer = recv_buf_idx - msg_bytes;

                    // Move the rest of the buffer over
                    memmove(recv_buf, recv_buf + msg_bytes, bytes_left_in_buffer);
                    recv_buf_idx = bytes_left_in_buffer;

                    // Zero the rest of the buffer just to be safe.
                    // This is just overhead if everything works as expected.
                    memset(recv_buf + bytes_left_in_buffer, 0, msg_bytes);

                    if (generic_msg->message_type == MSG_JOIN_ACK) {
                        join_ack_message_t *join_ack_msg = (join_ack_message_t *)generic_msg;
                        memcpy(rejoin_token, join_ack_msg->rejoin_token, REJOIN_TOKEN_LEN);
                        own_player_id = join_ack_msg->player_id;
                        global_own_player_id = own_player_id;

                        TraceLog(LOG_INFO, "Joined game with player id %d", own_player_id);

                        got_join_ack = 1;
                    } else if (generic_msg->message_type == MSG_CURRENT_PLAYERS) {
                        current_players_message_t *current_players_msg = (current_players_message_t *)generic_msg;

                        // Clear the previous player states, since this meassge acts as an initialization
                        clear_player_states(player_states);

                        for (int player_idx = 0; player_idx < current_players_msg->player_count; player_idx++) {
                            player_info_t player_info = current_players_msg->player_infos[player_idx];

                            player_state_t *player_state = player_state_new(player_info.player_id, player_info.name);
                            tree_insert(player_states, player_info.player_id, (void *)player_state);

                            // Prevent freeing the name later
                            player_info.name = NULL;
                        }

                        got_current_players = 1;
                    } else if (generic_msg->message_type == MSG_PLAYER_POSITIONS) {
                        player_positions_message_t *player_positions_msg = (player_positions_message_t *)generic_msg;
                        for (int player_idx = 0; player_idx < player_positions_msg->player_count; player_idx++) {
                            player_position_t player_pos = player_positions_msg->player_positions[player_idx];

                            player_state_t *player_state = tree_get(player_states, player_pos.player_id);
                            if (player_state != no_node_sentinel) {
                                player_state->pos = (Vector2){player_pos.x, player_pos.y};
                                player_state->mass = player_pos.mass;
                            }
                        }
                    } else if (generic_msg->message_type == MSG_PLAYER_JOIN) {
                        player_join_message_t *player_join_msg = (player_join_message_t *)generic_msg;

                        player_state_t *player_state = player_state_new(player_join_msg->player_info.player_id, player_join_msg->player_info.name);
                        player_state_t *prev_player_state = tree_insert(player_states, player_join_msg->player_info.player_id, player_state);
                        if (prev_player_state != no_node_sentinel) {
                            player_state_free(prev_player_state);
                        }

                        // Prevent freeing the name later
                        player_join_msg->player_info.name = NULL;
                    } else if (generic_msg->message_type == MSG_PLAYER_LEAVE) {
                        player_leave_message_t *player_leave_msg = (player_leave_message_t *)generic_msg;

                        TraceLog(LOG_DEBUG, "Got player leave message");

                        player_state_t *player_state = tree_remove(player_states, player_leave_msg->player_id);
                        if (player_state != no_node_sentinel) {
                            player_state_free(player_state);
                        } else {
                            TraceLog(LOG_WARNING, "Got player leave message, but the internal player state data structure did not contain that player");
                        }
                    }

                    // TODO: Handle game full

                    message_free(generic_msg);
                }
            }
        }

        switch (state) {
            case STATE_ENTER_NAME:
            {
                while ((chr = GetCharPressed())) {
                    if ((chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || (chr >= '0' && chr <= '9')) {
                        if (player_name_chars < MAX_PLAYER_NAME_LEN) {
                            player_name[player_name_chars] = chr;
                            player_name[player_name_chars + 1] = 0;
                            player_name_chars++;
                        }
                    }
                }

                while ((key = GetKeyPressed())) {
                    if ((key == KEY_ENTER || key == KEY_KP_ENTER) && player_name_chars > 0) {
                        TraceLog(LOG_INFO, "Joining game with name '%s'", player_name);

                        join_message_t join_msg = {
                            .message_type = MSG_JOIN,
                            .name = player_name,
                            .name_length = player_name_chars,
                        };

                        int msg_len = serialize_message((generic_message_t *)&join_msg, send_buf, SEND_BUF_LEN);
                        // TODO: Handle error
                        send_all(sock, send_buf, msg_len);

                        got_join_ack = 0;
                        got_current_players = 0;

                        state = STATE_JOINING;
                    } else if (key == KEY_BACKSPACE) {
                        if (player_name_chars > 0) {
                            player_name_chars--;
                            player_name[player_name_chars] = 0;
                        }
                    }
                }

                DrawText("Enter your name:", 0, 0, 24, WHITE);
                DrawText(player_name, 0, 24, 24, WHITE);

                break;
            }

            case STATE_JOINING:
            {
                if (got_join_ack && got_current_players) {
                    state = STATE_INGAME;
                }

                DrawText("Joining...", 0, 0, 24, WHITE);

                break;
            }

            case STATE_INGAME:
            {
                Vector2 mouse_pos = GetMousePosition();
                int is_inside_window = mouse_pos.x >= 0 && mouse_pos.x <= WINDOW_WIDTH && mouse_pos.y >= 0 && mouse_pos.y <= WINDOW_HEIGHT;
                if (is_inside_window && !Vector2Equals(previous_display_target, mouse_pos)) {
                    set_target_message_t set_target_msg = {
                        .message_type = MSG_SET_TARGET,
                        .x = mouse_pos.x * window_to_field_scale_factor,
                        .y = mouse_pos.y * window_to_field_scale_factor,
                    };

                    int msg_len = serialize_message((generic_message_t *)&set_target_msg, send_buf, SEND_BUF_LEN);
                    // TODO: Handle error
                    send_all(sock, send_buf, msg_len);

                    previous_display_target = mouse_pos;
                }

                DrawText("Welcome to AgarIO", 0, 0, 24, WHITE);

                tree_for_each_value(player_states, draw_player_pos);

                break;
            }
        }

        draw_fps();
		
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
