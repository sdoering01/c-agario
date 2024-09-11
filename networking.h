#ifndef NETWORKING_H
#define NETWORKING_H

#include <stdint.h>

int send_all(int sock, uint8_t *msg, int msg_len);

#endif // NETWORKING_H
