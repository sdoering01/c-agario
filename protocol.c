#include "protocol.h"

#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>

static uint16_t deserialize_uint16_t(uint8_t *buf) {
    uint16_t network_num;
    memcpy(&network_num, buf, 2);
    return ntohs(network_num);
}

static uint32_t deserialize_uint32_t(uint8_t *buf) {
    uint32_t network_num;
    memcpy(&network_num, buf, 4);
    return ntohl(network_num);
}

static float deserialize_float(uint8_t *buf) {
    return deserialize_uint32_t(buf) * 1.0 / (1 << 6);
}

static char *deserialize_string(uint8_t *buf, size_t char_count) {
    char *str = NULL;
    if (char_count > 0) {
        str = malloc(char_count + 1);
        memcpy(str, buf, char_count);
        str[char_count] = '\0';
    }
    return str;
}

static uint8_t *serialize_uint8_t(uint8_t *buf, uint8_t num) {
    *buf = num;
    return buf + 1;
}

static uint8_t *serialize_uint16_t(uint8_t *buf, uint16_t num) {
    uint16_t network_num = htons(num);
    memcpy(buf, &network_num, 2);
    return buf + 2;
}

static uint8_t *serialize_uint32_t(uint8_t *buf, uint32_t num) {
    uint32_t network_num = htonl(num);
    memcpy(buf, &network_num, 4);
    return buf + 4;
}

static uint8_t *serialize_float(uint8_t *buf, float num) {
    return serialize_uint32_t(buf, num * (1 << 6));
}

static uint8_t *serialize_memcpy(uint8_t *buf, void *data, uint16_t len) {
    memcpy(buf, data, len);
    return buf + len;
}

static bool is_valid_serialized_message(uint8_t *buf, uint16_t buf_len) {
    uint16_t msg_len, payload_len;
    uint8_t *payload;

    if (buf_len < 3) return false;

    msg_len = deserialize_uint16_t(buf);

    if (msg_len > buf_len) return false;

    payload_len = msg_len - 3;
    payload = buf + 3;

    switch (buf[2]) {
        case MSG_JOIN:
        {
            if (payload_len < 1) return false;
            uint8_t name_len = payload[0];
            if (name_len > MAX_PLAYER_NAME_LEN) return false;
            if (payload_len != 1 + name_len) return false;
            break;
        }

        case MSG_REJOIN:
        {
            if (payload_len != 4 + REJOIN_TOKEN_LEN) return false;
            break;
        }

        case MSG_LEAVE:
        {
            if (payload_len != 0) return false;
            break;
        }

        case MSG_SET_TARGET:
        {
            if (payload_len != 4 + 4) return false;
            break;
        }

        case MSG_JOIN_ACK:
        {
            if (payload_len != 4 + REJOIN_TOKEN_LEN) return false;
            break;
        }

        case MSG_CURRENT_PLAYERS:
        {
            if (payload_len < 2) return false;
            uint16_t player_count = deserialize_uint16_t(payload);
            uint16_t got_players = 0;
            payload += 2;
            payload_len -= 2;
            while (payload < buf + msg_len) {
                if (payload_len < 5) return false;
                got_players++;
                uint8_t name_len = payload[4];
                if (name_len > MAX_PLAYER_NAME_LEN) return false;
                if (payload_len < 5 + name_len) return false;
                payload += 5 + name_len;
                payload_len -= 5 + name_len;
            }
            if (got_players != player_count) return false;
            if (payload_len != 0) return false;
            break;
        }

        case MSG_PLAYER_JOIN:
        {
            if (payload_len < 5) return false;
            uint8_t name_len = payload[4];
            if (name_len > MAX_PLAYER_NAME_LEN) return false;
            if (payload_len != 5 + name_len) return false;
            break;
        }

        case MSG_PLAYER_LEAVE:
        {
            if (payload_len != 4) return false;
            break;
        }

        case MSG_PLAYER_POSITIONS:
        {
            if (payload_len < 2) return false;
            uint16_t player_count = deserialize_uint16_t(payload);
            if (payload_len != 2 + player_count * (4 + 4 + 4 + 4)) return false;
            break;
        }

        case MSG_SPAWNED_FOOD:
        {
            if (payload_len < 2) return false;
            uint16_t food_count = deserialize_uint16_t(payload);
            if (payload_len != 2 + food_count * (4 + 4 + 4)) return false;
            break;
        }

        case MSG_EATEN_FOOD:
        {
            if (payload_len < 2) return false;
            uint16_t food_count = deserialize_uint16_t(payload);
            if (payload_len != 2 + food_count * 4) return false;
            break;
        }

        case MSG_JOIN_ERROR:
        {
            if (payload_len < 2) return false;
            uint8_t error_code = payload[0];
            if (error_code != JOIN_ERR_GAME_FULL) return false;
            uint8_t error_message_length = payload[1];
            if (payload_len != 2 + error_message_length) return false; 
            break;
        }

        case MSG_KICK:
        {
            if (payload_len < 1) return false;
            uint8_t reason_len = payload[0];
            if (payload_len != 1 + reason_len) return false;
            break;
        }

        default:
            return false;
    }

    return true;
}

static int serialized_message_length(generic_message_t *generic_msg) {
    int len = 2 + 1;

    switch (generic_msg->message_type) {
        case MSG_JOIN:
        {
            join_message_t *msg = (join_message_t *)generic_msg;
            len += 1 + msg->name_length;
            break;
        }

        case MSG_REJOIN:
        {
            len += 4 + REJOIN_TOKEN_LEN;
            break;
        }

        case MSG_LEAVE:
        {
            break;
        }

        case MSG_SET_TARGET:
        {
            len += 4 + 4;
            break;
        }

        case MSG_JOIN_ACK:
        {
            len += 4 + REJOIN_TOKEN_LEN;
            break;
        }

        case MSG_CURRENT_PLAYERS:
        {
            current_players_message_t *msg = (current_players_message_t *)generic_msg;
            len += 2;
            for (int i = 0; i < msg->player_count; i++) {
                len += 4 + 1 + msg->player_infos[i].name_length;
            }
            break;
        }

        case MSG_PLAYER_JOIN:
        {
            player_join_message_t *msg = (player_join_message_t *)generic_msg;
            len += 4 + 1 + msg->player_info.name_length;
            break;
        }

        case MSG_PLAYER_LEAVE:
        {
            len += 4;
            break;
        }

        case MSG_PLAYER_POSITIONS:
        {
            player_positions_message_t *msg = (player_positions_message_t *)generic_msg;
            len += 2 + msg->player_count * (4 + 4 + 4 + 4);
            break;
        }

        case MSG_SPAWNED_FOOD:
        {
            spawned_food_message_t *msg = (spawned_food_message_t *)generic_msg;
            len += 2 + msg->food_count * (4 + 4 + 4);
            break;
        }

        case MSG_EATEN_FOOD:
        {
            eaten_food_message_t *msg = (eaten_food_message_t *)generic_msg;
            len += 2 + msg->food_count * 4;
            break;
        }

        case MSG_JOIN_ERROR:
        {
            join_error_message_t *msg = (join_error_message_t *)generic_msg;
            len += 2 + msg->error_message_length;
            break;
        }

        case MSG_KICK:
        {
            kick_message_t *msg = (kick_message_t *)generic_msg;
            len += 1 + msg->reason_length;
            break;
        }

        default:
            return 0;
    }

    return len;
}

int deserialize_message(uint8_t *buf, uint16_t len, generic_message_t **generic_msg) {
    uint8_t message_type, *payload;

    if (!is_valid_serialized_message(buf, len)) {
        return 0;
    }

    message_type = buf[2];
    payload = buf + 3;

    switch (message_type) {
        case MSG_JOIN:
        {
            join_message_t *msg = malloc(sizeof(join_message_t));
            msg->name_length = payload[0];
            msg->name = deserialize_string(payload + 1, msg->name_length);
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        case MSG_REJOIN:
        {
            rejoin_message_t *msg = malloc(sizeof(rejoin_message_t));
            msg->player_id = deserialize_uint32_t(payload);
            memcpy(msg->rejoin_token, payload + 4, REJOIN_TOKEN_LEN);
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        case MSG_LEAVE:
        {
            leave_message_t *msg = malloc(sizeof(leave_message_t));
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        case MSG_SET_TARGET:
        {
            set_target_message_t *msg = malloc(sizeof(set_target_message_t));
            msg->x = deserialize_float(payload);
            msg->y = deserialize_float(payload + 4);
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        case MSG_JOIN_ACK:
        {
            join_ack_message_t *msg = malloc(sizeof(join_ack_message_t));
            msg->player_id = deserialize_uint32_t(payload);
            memcpy(msg->rejoin_token, payload + 4, REJOIN_TOKEN_LEN);
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        case MSG_CURRENT_PLAYERS:
        {
            current_players_message_t *msg = malloc(sizeof(current_players_message_t));
            uint16_t player_count = deserialize_uint16_t(payload);
            msg->player_count = player_count;
            player_info_t *player_infos = NULL;
            if (player_count > 0) {
                player_infos = malloc(player_count * sizeof(player_info_t));
                payload += 2;
                for (int i = 0; i < player_count; i++) {
                    player_infos[i].player_id = deserialize_uint32_t(payload);
                    player_infos[i].name_length = payload[4];
                    player_infos[i].name = deserialize_string(payload + 5, player_infos[i].name_length);
                    payload += 5 + player_infos[i].name_length;
                }
            }
            msg->player_infos = player_infos;
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        case MSG_PLAYER_JOIN:
        {
            player_join_message_t *msg = malloc(sizeof(player_join_message_t));
            msg->player_info.player_id = deserialize_uint32_t(payload);
            msg->player_info.name_length = payload[4];
            msg->player_info.name = deserialize_string(payload + 5, msg->player_info.name_length);
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        case MSG_PLAYER_LEAVE:
        {
            player_leave_message_t *msg = malloc(sizeof(player_leave_message_t));
            msg->player_id = deserialize_uint32_t(payload);
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        case MSG_PLAYER_POSITIONS:
        {
            player_positions_message_t *msg = malloc(sizeof(player_positions_message_t));
            uint16_t player_count = deserialize_uint16_t(payload);
            msg->player_count = player_count;
            player_position_t *player_positions = NULL;
            if (player_count > 0) {
                player_positions = malloc(player_count * sizeof(player_position_t));
                payload += 2;
                for (int i = 0; i < player_count; i++) {
                    player_positions[i].player_id = deserialize_uint32_t(payload);
                    player_positions[i].x = deserialize_float(payload + 4);
                    player_positions[i].y = deserialize_float(payload + 8);
                    player_positions[i].mass = deserialize_uint32_t(payload + 12);
                    payload += 16;
                }
            }
            msg->player_positions = player_positions;
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        case MSG_SPAWNED_FOOD:
        {
            spawned_food_message_t *msg = malloc(sizeof(spawned_food_message_t));
            uint16_t food_count = deserialize_uint16_t(payload);
            msg->food_count = food_count;
            food_position_t *food_positions = NULL;
            if (food_count > 0) {
                food_positions = malloc(food_count *  sizeof(food_position_t));
                payload += 2;
                for (int i = 0; i < food_count; i++) {
                    food_positions[i].food_id = deserialize_uint32_t(payload);
                    food_positions[i].x = deserialize_float(payload + 4);
                    food_positions[i].y = deserialize_float(payload + 8);
                    payload += 12;
                }
            }
            msg->food_positions = food_positions;
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        case MSG_EATEN_FOOD:
        {
            eaten_food_message_t *msg = malloc(sizeof(eaten_food_message_t));
            uint16_t food_count = deserialize_uint16_t(payload);
            msg->food_count = food_count;
            uint32_t *food_ids = NULL;
            if (food_count > 0) {
                food_ids = malloc(food_count * sizeof(uint32_t));
                payload += 2;
                for (int i = 0; i < food_count; i++) {
                    food_ids[i] = deserialize_uint32_t(payload);
                    payload += 4;
                }
            }
            msg->food_ids = food_ids;
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        case MSG_JOIN_ERROR:
        {
            join_error_message_t *msg = malloc(sizeof(join_error_message_t));
            msg->error_code = payload[0];
            msg->error_message_length = payload[1];
            msg->error_message = deserialize_string(payload + 2, msg->error_message_length);
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        case MSG_KICK:
        {
            kick_message_t *msg = malloc(sizeof(kick_message_t));
            msg->reason_length = payload[0];
            msg->reason = deserialize_string(payload + 1, msg->reason_length);
            *generic_msg = (generic_message_t *)msg;
            break;
        }

        default:
            return 0;
    }

    (*generic_msg)->message_type = message_type;

    // Could be optimized a little further, but this way we only have the length
    // calculation in one place, which is more robust
    return serialized_message_length(*generic_msg);
}

int serialize_message(generic_message_t *generic_msg, uint8_t *buf, uint16_t buf_len) {
    int msg_len = 0;
    uint8_t *orig_buf = buf;

    if (!generic_msg || !buf) {
        return -1;
    }

    msg_len = serialized_message_length(generic_msg);
    if (msg_len > (1 << 16) - 1 || msg_len > buf_len) {
        return -1;
    }

    buf = serialize_uint16_t(buf, msg_len);
    buf = serialize_uint8_t(buf, generic_msg->message_type);

    switch (generic_msg->message_type) {
        case MSG_JOIN:
        {
            join_message_t *msg = (join_message_t *)generic_msg;
            buf = serialize_uint8_t(buf, msg->name_length);
            buf = serialize_memcpy(buf, msg->name, msg->name_length);
            break;
        }

        case MSG_REJOIN:
        {
            rejoin_message_t *msg = (rejoin_message_t *)generic_msg;
            buf = serialize_uint32_t(buf, msg->player_id);
            buf = serialize_memcpy(buf, msg-> rejoin_token, REJOIN_TOKEN_LEN);
            break;
        }

        case MSG_LEAVE:
        {
            break;
        }

        case MSG_SET_TARGET:
        {
            set_target_message_t *msg = (set_target_message_t *)generic_msg;
            buf = serialize_float(buf, msg->x);
            buf = serialize_float(buf, msg->y);
            break;
        }

        case MSG_JOIN_ACK:
        {
            join_ack_message_t *msg = (join_ack_message_t *)generic_msg;
            buf = serialize_uint32_t(buf, msg->player_id);
            buf = serialize_memcpy(buf, msg->rejoin_token, REJOIN_TOKEN_LEN);
            break;
        }

        case MSG_CURRENT_PLAYERS:
        {
            current_players_message_t *msg = (current_players_message_t *)generic_msg;
            buf = serialize_uint16_t(buf, msg->player_count);
            for (int i = 0; i < msg->player_count; i++) {
                buf = serialize_uint32_t(buf, msg->player_infos[i].player_id); 
                buf = serialize_uint8_t(buf, msg->player_infos[i].name_length); 
                buf = serialize_memcpy(buf, msg->player_infos[i].name,
                                       msg->player_infos[i].name_length);
            };
            break;
        }

        case MSG_PLAYER_JOIN:
        {
            player_join_message_t *msg = (player_join_message_t *)generic_msg;
            buf = serialize_uint32_t(buf, msg->player_info.player_id);
            buf = serialize_uint8_t(buf, msg->player_info.name_length);
            buf = serialize_memcpy(buf, msg->player_info.name, msg->player_info.name_length);
            break;
        }

        case MSG_PLAYER_LEAVE:
        {
            player_leave_message_t *msg = (player_leave_message_t *)generic_msg;
            buf = serialize_uint32_t(buf, msg->player_id);
            break;
        }

        case MSG_PLAYER_POSITIONS:
        {
            player_positions_message_t *msg = (player_positions_message_t *)generic_msg;
            buf = serialize_uint16_t(buf, msg->player_count);
            for (int i = 0; i < msg->player_count; i++) {
                buf = serialize_uint32_t(buf, msg->player_positions[i].player_id);
                buf = serialize_float(buf, msg->player_positions[i].x);
                buf = serialize_float(buf, msg->player_positions[i].y);
                buf = serialize_uint32_t(buf, msg->player_positions[i].mass);
            }
            break;
        }

        case MSG_SPAWNED_FOOD:
        {
            spawned_food_message_t *msg = (spawned_food_message_t *)generic_msg;
            buf = serialize_uint16_t(buf, msg->food_count);
            for (int i = 0; i < msg->food_count; i++) {
                buf = serialize_uint32_t(buf, msg->food_positions[i].food_id);
                buf = serialize_float(buf, msg->food_positions[i].x);
                buf = serialize_float(buf, msg->food_positions[i].y);
            }
            break;
        }

        case MSG_EATEN_FOOD:
        {
            eaten_food_message_t *msg = (eaten_food_message_t *)generic_msg;
            buf = serialize_uint16_t(buf, msg->food_count);
            for (int i = 0; i < msg->food_count; i++) {
                buf = serialize_uint32_t(buf, msg->food_ids[i]);
            }
            break;
        }

        case MSG_JOIN_ERROR:
        {
            join_error_message_t *msg = (join_error_message_t *)generic_msg;
            buf = serialize_uint8_t(buf, msg->error_code);
            buf = serialize_uint8_t(buf, msg->error_message_length);
            buf = serialize_memcpy(buf, msg->error_message, msg->error_message_length);
            break;
        }

        case MSG_KICK:
        {
            kick_message_t *msg = (kick_message_t *)generic_msg;
            buf = serialize_uint8_t(buf, msg->reason_length);
            buf = serialize_memcpy(buf, msg->reason, msg->reason_length);
            break;
        }

        default:
            return -1;
    }

    if (buf - orig_buf != msg_len) {
        printf("serialized message length is wrong: %ld (should be %d)\n", buf - orig_buf, msg_len);
        return -1;
    }

    return msg_len;
}

void message_free(generic_message_t *generic_msg) {
    if (!generic_msg) {
        return;
    }

    switch (generic_msg->message_type) {
        case MSG_JOIN:
        {
            join_message_t *msg = (join_message_t *)generic_msg;
            free(msg->name);
            break;
        }

        case MSG_CURRENT_PLAYERS:
        {
            current_players_message_t *msg = (current_players_message_t *)generic_msg;
            for (int i = 0; i < msg->player_count; i++) {
                free(msg->player_infos[i].name);
            }
            free(msg->player_infos);
            break;
        }

        case MSG_PLAYER_JOIN:
        {
            player_join_message_t *msg = (player_join_message_t *)generic_msg;
            free(msg->player_info.name);
            break;
        }

        case MSG_PLAYER_POSITIONS:
        {
            player_positions_message_t *msg = (player_positions_message_t *)generic_msg;
            free(msg->player_positions);
            break;
        }
        
        case MSG_SPAWNED_FOOD:
        {
            spawned_food_message_t *msg = (spawned_food_message_t *)generic_msg;
            free(msg->food_positions);
            break;
        }

        case MSG_EATEN_FOOD:
        {
            eaten_food_message_t *msg = (eaten_food_message_t *)generic_msg;
            free(msg->food_ids);
            break;
        }

        case MSG_JOIN_ERROR:
        {
            join_error_message_t *msg = (join_error_message_t *)generic_msg;
            free(msg->error_message);
            break;
        }

        case MSG_KICK:
        {
            kick_message_t *msg = (kick_message_t *)generic_msg;
            free(msg->reason);
            break;
        }
    }

    free(generic_msg);
}
