#include "unity/unity.h"
#include "../protocol.h"

#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 65535
static uint8_t buf[BUF_SIZE];
static uint32_t player_id;
static rejoin_token_t rejoin_token;

void setUp(void) {
    memset(buf, 0, BUF_SIZE);
    player_id = 0x12345678;
    for (int i = 0; i < REJOIN_TOKEN_LEN; i++) {
        rejoin_token[i] = i;
    }
}

void tearDown(void) {
}

void test_join_message(void) {
    size_t len;
    join_message_t *msg = malloc(sizeof(join_message_t));

    msg->message_type = MSG_JOIN;
    msg->name_length = 5;
    msg->name = malloc(6);
    strncpy(msg->name, "Simon", 6);

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(9, len);

    join_message_t *msg2 = (join_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_JOIN, msg2->message_type);
    TEST_ASSERT_EQUAL(5, msg2->name_length);
    TEST_ASSERT_EQUAL_STRING("Simon", msg2->name);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_rejoin_message(void) {
    size_t len;
    rejoin_message_t *msg = malloc(sizeof(rejoin_message_t));

    msg->message_type = MSG_REJOIN;
    msg->player_id = player_id;
    memcpy(msg->rejoin_token, rejoin_token, REJOIN_TOKEN_LEN);

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(7 + REJOIN_TOKEN_LEN, len);

    rejoin_message_t *msg2 = (rejoin_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_REJOIN, msg2->message_type);
    TEST_ASSERT_EQUAL(player_id, msg2->player_id);
    TEST_ASSERT_EQUAL_MEMORY(rejoin_token, msg2->rejoin_token, REJOIN_TOKEN_LEN);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_leave_message(void) {
    size_t len;
    leave_message_t *msg = malloc(sizeof(leave_message_t));

    msg->message_type = MSG_LEAVE;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(3, len);

    leave_message_t *msg2 = (leave_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_LEAVE, msg2->message_type);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_set_target_message(void) {
    size_t len;
    set_target_message_t *msg = malloc(sizeof(set_target_message_t));

    msg->message_type = MSG_SET_TARGET;
    msg->x = 111.111;
    msg->y = 222.222;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(11, len);

    set_target_message_t *msg2 = (set_target_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_SET_TARGET, msg2->message_type);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 111.111, msg2->x);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 222.222, msg2->y);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_join_ack_message(void) {
    size_t len;
    join_ack_message_t *msg = malloc(sizeof(join_ack_message_t));

    msg->message_type = MSG_JOIN_ACK;
    msg->player_id = player_id;
    memcpy(msg->rejoin_token, rejoin_token, REJOIN_TOKEN_LEN);

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(7 + REJOIN_TOKEN_LEN, len);

    join_ack_message_t *msg2 = (join_ack_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_JOIN_ACK, msg2->message_type);
    TEST_ASSERT_EQUAL(player_id, msg2->player_id);
    TEST_ASSERT_EQUAL_MEMORY(rejoin_token, msg2->rejoin_token, REJOIN_TOKEN_LEN);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_current_players_message(void) {
    size_t len;
    current_players_message_t *msg = malloc(sizeof(current_players_message_t));

    msg->message_type = MSG_CURRENT_PLAYERS;
    msg->player_count = 3;
    msg->player_infos = malloc(3 * sizeof(player_info_t));
    msg->player_infos[0].player_id = 0x12345678;
    msg->player_infos[0].name_length = 5;
    msg->player_infos[0].name = malloc(6);
    strncpy(msg->player_infos[0].name, "Simon", 6);
    msg->player_infos[1].player_id = 0x87654321;
    msg->player_infos[1].name_length = 4;
    msg->player_infos[1].name = malloc(5);
    strncpy(msg->player_infos[1].name, "John", 5);
    msg->player_infos[2].player_id = 0x12344321;
    msg->player_infos[2].name_length = 0;
    msg->player_infos[2].name = NULL;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(29, len);

    current_players_message_t *msg2 = (current_players_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_CURRENT_PLAYERS, msg2->message_type);
    TEST_ASSERT_EQUAL(3, msg2->player_count);
    TEST_ASSERT_EQUAL(0x12345678, msg2->player_infos[0].player_id);
    TEST_ASSERT_EQUAL(5, msg2->player_infos[0].name_length);
    TEST_ASSERT_EQUAL_STRING("Simon", msg2->player_infos[0].name);
    TEST_ASSERT_EQUAL(0x87654321, msg2->player_infos[1].player_id);
    TEST_ASSERT_EQUAL(4, msg2->player_infos[1].name_length);
    TEST_ASSERT_EQUAL_STRING("John", msg2->player_infos[1].name);
    TEST_ASSERT_EQUAL(0x12344321, msg2->player_infos[2].player_id);
    TEST_ASSERT_EQUAL(0, msg2->player_infos[2].name_length);
    TEST_ASSERT_NULL(msg2->player_infos[2].name);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_empty_current_players_message(void) {
    size_t len;
    current_players_message_t *msg = malloc(sizeof(current_players_message_t));

    msg->message_type = MSG_CURRENT_PLAYERS;
    msg->player_count = 0;
    msg->player_infos = NULL;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(5, len);

    current_players_message_t *msg2 = (current_players_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_CURRENT_PLAYERS, msg2->message_type);
    TEST_ASSERT_EQUAL(0, msg2->player_count);
    TEST_ASSERT_NULL(msg2->player_infos);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_player_join_message(void) {
    size_t len;
    player_join_message_t *msg = malloc(sizeof(player_join_message_t));

    msg->message_type = MSG_PLAYER_JOIN;
    msg->player_info.player_id = player_id;
    msg->player_info.name_length = 5;
    msg->player_info.name = malloc(6);
    strncpy(msg->player_info.name, "Simon", 6);

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(13, len);

    player_join_message_t *msg2 = (player_join_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_PLAYER_JOIN, msg2->message_type);
    TEST_ASSERT_EQUAL(player_id, msg2->player_info.player_id);
    TEST_ASSERT_EQUAL(5, msg2->player_info.name_length);
    TEST_ASSERT_EQUAL_STRING("Simon", msg2->player_info.name);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_player_leave_message(void) {
    size_t len;
    player_leave_message_t *msg = malloc(sizeof(player_leave_message_t));

    msg->message_type = MSG_PLAYER_LEAVE;
    msg->player_id = player_id;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(7, len);

    player_leave_message_t *msg2 = (player_leave_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_PLAYER_LEAVE, msg2->message_type);
    TEST_ASSERT_EQUAL(player_id, msg2->player_id);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_player_positions_message(void) {
    size_t len;
    player_positions_message_t *msg = malloc(sizeof(player_positions_message_t));

    msg->message_type = MSG_PLAYER_POSITIONS;
    msg->player_count = 3;
    msg->player_positions = malloc(3 * sizeof(player_position_t));
    msg->player_positions[0].player_id = 0x12345678;
    msg->player_positions[0].x = 1.0;
    msg->player_positions[0].y = 2.0;
    msg->player_positions[0].mass = 100;
    msg->player_positions[1].player_id = 0x87654321;
    msg->player_positions[1].x = 3.0;
    msg->player_positions[1].y = 4.0;
    msg->player_positions[1].mass = 200;
    msg->player_positions[2].player_id = 0x12344321;
    msg->player_positions[2].x = 5.0;
    msg->player_positions[2].y = 6.0;
    msg->player_positions[2].mass = 300;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(53, len);

    player_positions_message_t *msg2 = (player_positions_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_PLAYER_POSITIONS, msg2->message_type);
    TEST_ASSERT_EQUAL(3, msg2->player_count);
    TEST_ASSERT_EQUAL(0x12345678, msg2->player_positions[0].player_id);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 1.0, msg2->player_positions[0].x);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 2.0, msg2->player_positions[0].y);
    TEST_ASSERT_EQUAL(100, msg2->player_positions[0].mass);
    TEST_ASSERT_EQUAL(0x87654321, msg2->player_positions[1].player_id);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 3.0, msg2->player_positions[1].x);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 4.0, msg2->player_positions[1].y);
    TEST_ASSERT_EQUAL(200, msg2->player_positions[1].mass);
    TEST_ASSERT_EQUAL(0x12344321, msg2->player_positions[2].player_id);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 5.0, msg2->player_positions[2].x);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 6.0, msg2->player_positions[2].y);
    TEST_ASSERT_EQUAL(300, msg2->player_positions[2].mass);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_empty_player_positions_message(void) {
    size_t len;
    player_positions_message_t *msg = malloc(sizeof(player_positions_message_t));

    msg->message_type = MSG_PLAYER_POSITIONS;
    msg->player_count = 0;
    msg->player_positions = NULL;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(5, len);

    player_positions_message_t *msg2 = (player_positions_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_PLAYER_POSITIONS, msg2->message_type);
    TEST_ASSERT_EQUAL(0, msg2->player_count);
    TEST_ASSERT_NULL(msg2->player_positions);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_spawned_food_message(void) {
    size_t len;
    spawned_food_message_t *msg = malloc(sizeof(spawned_food_message_t));

    msg->message_type = MSG_SPAWNED_FOOD;
    msg->food_count = 3;
    msg->food_positions = malloc(3 * sizeof(food_position_t));
    msg->food_positions[0].food_id = 0x12345678;
    msg->food_positions[0].x = 1.0;
    msg->food_positions[0].y = 2.0;
    msg->food_positions[1].food_id = 0x87654321;
    msg->food_positions[1].x = 3.0;
    msg->food_positions[1].y = 4.0;
    msg->food_positions[2].food_id = 0x12344321;
    msg->food_positions[2].x = 5.0;
    msg->food_positions[2].y = 6.0;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(41, len);

    spawned_food_message_t *msg2 = (spawned_food_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_SPAWNED_FOOD, msg2->message_type);
    TEST_ASSERT_EQUAL(3, msg2->food_count);
    TEST_ASSERT_EQUAL(0x12345678, msg2->food_positions[0].food_id);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 1.0, msg2->food_positions[0].x);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 2.0, msg2->food_positions[0].y);
    TEST_ASSERT_EQUAL(0x87654321, msg2->food_positions[1].food_id);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 3.0, msg2->food_positions[1].x);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 4.0, msg2->food_positions[1].y);
    TEST_ASSERT_EQUAL(0x12344321, msg2->food_positions[2].food_id);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 5.0, msg2->food_positions[2].x);
    TEST_ASSERT_FLOAT_WITHIN(5e-2, 6.0, msg2->food_positions[2].y);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_empty_spawned_food_message(void) {
    size_t len;
    spawned_food_message_t *msg = malloc(sizeof(spawned_food_message_t));

    msg->message_type = MSG_SPAWNED_FOOD;
    msg->food_count = 0;
    msg->food_positions = NULL;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(5, len);

    spawned_food_message_t *msg2 = (spawned_food_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_SPAWNED_FOOD, msg2->message_type);
    TEST_ASSERT_EQUAL(0, msg2->food_count);
    TEST_ASSERT_NULL(msg2->food_positions);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_eaten_food_message(void) {
    size_t len;
    eaten_food_message_t *msg = malloc(sizeof(eaten_food_message_t));

    msg->message_type = MSG_EATEN_FOOD;
    msg->food_count = 3;
    msg->food_ids = malloc(3 * sizeof(uint32_t));
    msg->food_ids[0] = 0x12345678;
    msg->food_ids[1] = 0x87654321;
    msg->food_ids[2] = 0x12344321;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(17, len);

    eaten_food_message_t *msg2 = (eaten_food_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_EATEN_FOOD, msg2->message_type);
    TEST_ASSERT_EQUAL(3, msg2->food_count);
    TEST_ASSERT_EQUAL(0x12345678, msg2->food_ids[0]);
    TEST_ASSERT_EQUAL(0x87654321, msg2->food_ids[1]);
    TEST_ASSERT_EQUAL(0x12344321, msg2->food_ids[2]);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_empty_eaten_food_message(void) {
    size_t len;
    eaten_food_message_t *msg = malloc(sizeof(eaten_food_message_t));

    msg->message_type = MSG_EATEN_FOOD;
    msg->food_count = 0;
    msg->food_ids = NULL;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(5, len);

    eaten_food_message_t *msg2 = (eaten_food_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_EATEN_FOOD, msg2->message_type);
    TEST_ASSERT_EQUAL(0, msg2->food_count);
    TEST_ASSERT_NULL(msg2->food_ids);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_join_error_message(void) {
    size_t len;
    join_error_message_t *msg = malloc(sizeof(join_error_message_t));

    msg->message_type = MSG_JOIN_ERROR;
    msg->error_code = JOIN_ERR_GAME_FULL;
    msg->error_message_length = 12;
    msg->error_message = malloc(13);
    strncpy(msg->error_message, "Game is full", 13);

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(17, len);

    join_error_message_t *msg2 = (join_error_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_JOIN_ERROR, msg2->message_type);
    TEST_ASSERT_EQUAL(JOIN_ERR_GAME_FULL, msg2->error_code);
    TEST_ASSERT_EQUAL(12, msg2->error_message_length);
    TEST_ASSERT_EQUAL_STRING("Game is full", msg2->error_message);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_empty_join_error_message(void) {
    size_t len;
    join_error_message_t *msg = malloc(sizeof(join_error_message_t));

    msg->message_type = MSG_JOIN_ERROR;
    msg->error_code = JOIN_ERR_GAME_FULL;
    msg->error_message_length = 0;
    msg->error_message = NULL;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(5, len);

    join_error_message_t *msg2 = (join_error_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_JOIN_ERROR, msg2->message_type);
    TEST_ASSERT_EQUAL(JOIN_ERR_GAME_FULL, msg2->error_code);
    TEST_ASSERT_EQUAL(0, msg2->error_message_length);
    TEST_ASSERT_NULL(msg2->error_message);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_kick_message(void) {
    size_t len;
    kick_message_t *msg = malloc(sizeof(kick_message_t));

    msg->message_type = MSG_KICK;
    msg->reason_length = 29;
    msg->reason = malloc(30);
    strncpy(msg->reason, "You were kicked from the game", 30);

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(33, len);

    kick_message_t *msg2 = (kick_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_KICK, msg2->message_type);
    TEST_ASSERT_EQUAL(29, msg2->reason_length);
    TEST_ASSERT_EQUAL_STRING("You were kicked from the game", msg2->reason);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

void test_empty_kick_message(void) {
    size_t len;
    kick_message_t *msg = malloc(sizeof(kick_message_t));

    msg->message_type = MSG_KICK;
    msg->reason_length = 0;
    msg->reason = NULL;

    len = serialize_message((generic_message_t *)msg, buf, BUF_SIZE);
    TEST_ASSERT_EQUAL(4, len);

    kick_message_t *msg2 = (kick_message_t *)deserialize_message(buf, len);
    TEST_ASSERT_EQUAL(MSG_KICK, msg2->message_type);
    TEST_ASSERT_EQUAL(0, msg2->reason_length);
    TEST_ASSERT_NULL(msg2->reason);

    message_free((generic_message_t *)msg);
    message_free((generic_message_t *)msg2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_join_message);
    RUN_TEST(test_rejoin_message);
    RUN_TEST(test_leave_message);
    RUN_TEST(test_set_target_message);
    RUN_TEST(test_join_ack_message);
    RUN_TEST(test_current_players_message);
    RUN_TEST(test_empty_current_players_message);
    RUN_TEST(test_player_join_message);
    RUN_TEST(test_player_leave_message);
    RUN_TEST(test_player_positions_message);
    RUN_TEST(test_empty_player_positions_message);
    RUN_TEST(test_spawned_food_message);
    RUN_TEST(test_empty_spawned_food_message);
    RUN_TEST(test_eaten_food_message);
    RUN_TEST(test_empty_eaten_food_message);
    RUN_TEST(test_join_error_message);
    RUN_TEST(test_empty_join_error_message);
    RUN_TEST(test_kick_message);
    RUN_TEST(test_empty_kick_message);
    return UNITY_END();
}
