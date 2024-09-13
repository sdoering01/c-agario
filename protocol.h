#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define MAX_PLAYER_NAME_LEN 20
#define DEFAULT_PLAYER_NAME "Unnamed Player"
#define DEFAULT_PLAYER_NAME_LENGTH 14

#define REJOIN_TOKEN_LEN 16
#define MAX_REASON_MESSAGE_LEN 255

typedef uint8_t rejoin_token_t[REJOIN_TOKEN_LEN];

typedef struct generic_message_t {
    uint8_t message_type;
} generic_message_t;

// TODO: Align structs with padding

// Messages from client
#define MSG_JOIN 1
typedef struct join_message_t {
    uint8_t message_type;
    uint8_t name_length;
    char *name;
} join_message_t;

#define MSG_REJOIN 2
typedef struct rejoin_message_t {
    uint8_t message_type;
    uint32_t player_id;
    rejoin_token_t rejoin_token;
} rejoin_message_t;

#define MSG_LEAVE 3
typedef struct leave_message_t {
    uint8_t message_type;
} leave_message_t;

#define MSG_SET_TARGET 4
typedef struct set_target_message_t {
    uint8_t message_type;
    float x;
    float y;
} set_target_message_t;

// Messages from server
#define MSG_JOIN_ACK 33
typedef struct join_ack_message_t {
    uint8_t message_type;
    uint32_t player_id;
    rejoin_token_t rejoin_token;
} join_ack_message_t;

#define MSG_CURRENT_PLAYERS 34
typedef struct player_info_t {
    uint32_t player_id;
    uint8_t name_length;
    char *name;
} player_info_t;

typedef struct current_players_message_t {
    uint8_t message_type;
    uint16_t player_count;
    player_info_t *player_infos;
} current_players_message_t;

#define MSG_PLAYER_JOIN 35
typedef struct player_join_message_t {
    uint8_t message_type;
    player_info_t player_info;
} player_join_message_t;

#define MSG_PLAYER_LEAVE 36
typedef struct player_leave_message_t {
    uint8_t message_type;
    uint32_t player_id;
} player_leave_message_t;

#define MSG_PLAYER_POSITIONS 37
typedef struct player_position_t {
    uint32_t player_id;
    float x;
    float y;
    uint32_t mass;
} player_position_t;

typedef struct player_positions_message_t {
    uint8_t message_type;
    uint16_t player_count;
    player_position_t *player_positions;
} player_positions_message_t;

#define MSG_SPAWNED_FOOD 38
typedef struct food_position_t {
    uint32_t food_id;
    float x;
    float y;
} food_position_t;

typedef struct spawned_food_message_t {
    uint8_t message_type;
    uint16_t food_count;
    food_position_t *food_positions;
} spawned_food_message_t;

#define MSG_EATEN_FOOD 39
typedef struct eaten_food_message_t {
    uint8_t message_type;
    uint16_t food_count;
    uint32_t *food_ids;
} eaten_food_message_t;

#define MSG_JOIN_ERROR 40
#define JOIN_ERR_GAME_FULL 1
#define GAME_FULL_ERROR_MSG "The game is full"
typedef struct join_error_message_t {
    uint8_t message_type;
    uint8_t error_code;
    uint8_t error_message_length;
    char *error_message;
} join_error_message_t;

#define MSG_KICK 41
typedef struct kick_message_t {
    uint8_t message_type;
    uint8_t reason_length;
    char *reason;
} kick_message_t;

/*
 * Deserializes a message from the given buffer and stores it at `*generic_msg`.
 *
 * Returns the amount of bytes that were read from the buffer on success, and 0
 * on error. In the error case, `*generic_msg` is left untouched.
 *
 * Trying to deserialize a message from a buffer that does not contain a message
 * or only part of a message, is treated as an error.
 *
 * Note: The caller should free the message and its contents.
 */
int deserialize_message(uint8_t *buf, uint16_t len, generic_message_t **generic_msg);
int serialize_message(generic_message_t *msg, uint8_t *buf, uint16_t buf_len);
void message_free(generic_message_t *msg);

#endif // PROTOCOL_H
