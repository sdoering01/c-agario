#include <errno.h>
#include <sys/socket.h>

#include "networking.h"

int send_all(int sock, uint8_t *msg, int msg_len) {
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
