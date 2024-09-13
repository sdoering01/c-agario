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

#include "protocol.h"
#include "networking.h"

#define SEND_BUF_LEN 65535
#define RECV_BUF_LEN 65535

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 600

#define STATE_ENTER_NAME 1
#define STATE_INGAME 2

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
    Vector2 own_display_position = {-1e6, -1e6};

    // TODO: Get the field size from server
    float field_to_window_scale_factor = (float)(WINDOW_WIDTH) / 1000.0;
    float window_to_field_scale_factor = 1 / field_to_window_scale_factor;

	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);

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

	while (!WindowShouldClose())
	{
		BeginDrawing();

		ClearBackground(BLACK);

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

                        int got_join_ack = 0;
                        while (!got_join_ack) {
                            int space_in_buffer = RECV_BUF_LEN - recv_buf_idx;
                            n_bytes = recv(sock, recv_buf + recv_buf_idx, space_in_buffer, 0);

                            if (n_bytes == -1) {
                                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                    usleep(1e4);
                                } else if (errno != EINTR) {
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

                                        TraceLog(LOG_INFO, "Joined game with player id %d", own_player_id);

                                        got_join_ack = 1;
                                    }

                                    // TODO: Read current players message

                                    // TODO: Handle game full

                                    message_free(generic_msg);
                                }
                            }
                        }

                        state = STATE_INGAME;
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

            case STATE_INGAME:
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

                        if (generic_msg->message_type == MSG_PLAYER_POSITIONS) {
                            player_positions_message_t *player_positions_msg = (player_positions_message_t *)generic_msg;
                            for (int player_idx = 0; player_idx < player_positions_msg->player_count; player_idx++) {
                                player_position_t player_pos = player_positions_msg->player_positions[player_idx];
                                if (player_pos.player_id == own_player_id) {

                                    own_display_position.x = player_pos.x * field_to_window_scale_factor;
                                    own_display_position.y = player_pos.y * field_to_window_scale_factor;
                                }
                            }
                        }

                        message_free(generic_msg);
                    }
                }

                Vector2 mouse_pos = GetMousePosition();
                int is_inside_window = mouse_pos.x >= 0 && mouse_pos.x <= WINDOW_WIDTH && mouse_pos.y >= 0 && mouse_pos.y <= WINDOW_HEIGHT;
                if (is_inside_window) {
                    // TODO: Only do this, if the target has changed -- no need to send the same target all the time

                    set_target_message_t set_target_msg = {
                        .message_type = MSG_SET_TARGET,
                        .x = mouse_pos.x * window_to_field_scale_factor,
                        .y = mouse_pos.y * window_to_field_scale_factor,
                    };


                    int msg_len = serialize_message((generic_message_t *)&set_target_msg, send_buf, SEND_BUF_LEN);
                    // TODO: Handle error
                    send_all(sock, send_buf, msg_len);
                }

                DrawText("Welcome to AgarIO", 0, 0, 24, WHITE);

                // TODO: Show other players

                DrawCircleV(own_display_position, 50, WHITE);

                break;
            }
        }

        draw_fps();
		
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
