#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>

#include "raylib.h"

#include "protocol.h"
#include "networking.h"

#define SEND_BUF_LEN 65535

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 600

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

    char *name = "Simon";
    join_message_t join_msg = {
        .message_type = MSG_JOIN,
        .name = name,
        .name_length = strlen(name),
    };

    int msg_len = serialize_message((generic_message_t *)&join_msg, send_buf, SEND_BUF_LEN);
    send_all(sock, send_buf, msg_len);

	while (!WindowShouldClose())
	{
		BeginDrawing();

		ClearBackground(BLACK);
		DrawText("Welcome to AgarIO", 0, 0, 24, WHITE);
        DrawCircle(50, 50, 20.0, WHITE);
        draw_fps();
		
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
