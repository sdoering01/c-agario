#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>

#define MAX_EVENTS 5
#define MAX_CONNECTIONS 64

static char *too_many_connections_message = "too_many_connections";

typedef struct context {
    int epoll_fd;
    int connections[MAX_CONNECTIONS];
} context_t;

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

static void disconnect_socket(int sock, context_t *ctx) {
    int idx = 0;
    while (idx < MAX_CONNECTIONS && ctx->connections[idx] != sock) {
        idx++;
    }

    if (idx < MAX_CONNECTIONS) {
        ctx->connections[idx] = 0;
    }

    epoll_ctl(ctx->epoll_fd, EPOLL_CTL_DEL, sock, NULL);
    close(sock);
}

static int track_socket(int sock, context_t *ctx) {
    struct epoll_event event = {};
    int ret, idx = 0;

    while (idx < MAX_CONNECTIONS && ctx->connections[idx] != 0) {
        idx++;
    }

    if (idx == MAX_CONNECTIONS) {
        printf("Got too many connections");
        // TODO: Swap for binary error message once protocol is established
        send_all(sock, too_many_connections_message, strlen(too_many_connections_message));
        close(sock);
        return -1;
    }

    event.events = EPOLLIN;
    event.data.fd = sock;
    ret = epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, sock, &event);
    if (ret == -1) {
        printf("Error adding client_sock to epoll instance, closing socket\n");
        close(sock);
    } else {
        ctx->connections[idx] = sock;
    }

    return ret;
}

static void broadcast(char *msg, int msg_len, context_t *ctx) {
    int client_sock;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        client_sock = ctx->connections[i];
        if (client_sock) {
            if (send_all(client_sock, msg, msg_len) == -1) {
                printf("Error when sending to socket: errno %d -- %s\n", errno, strerror(errno));
                printf("Closing socket\n");
                disconnect_socket(client_sock, ctx);
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
    // The fd that is set here will be available when the event is received
    event.data.fd = server_sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sock, &event)) {
        printf("Error adding server_sock to epoll instance\n");
        close(server_sock);
        close(epoll_fd);
        return 1;
    }

    while (running) {
        event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (i = 0; i < event_count; i++) {
            if (events[i].data.fd == server_sock) {
                client_sock = accept(server_sock, NULL, NULL);
                if (client_sock < 0) {
                    printf("Couldn't accept connection...\n");
                    continue;
                }
                if (track_socket(client_sock, &ctx)) {
                    // In case something follows this
                    continue;
                }
            } else {
                client_sock = events[i].data.fd;

                bytes_received = recv(client_sock, client_message, sizeof(client_message), 0);
                // Be careful when handling errno, because calls to printf can overwrite it
                if (bytes_received == -1 && errno != EINTR) {
                    printf("Error when receiving from socket: errno %d -- %s\n", errno, strerror(errno));
                    printf("Closing socket\n");
                    disconnect_socket(client_sock, &ctx);
                    continue;
                }
                // Closed orderly by peer
                if (bytes_received == 0) {
                    disconnect_socket(client_sock, &ctx);
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
