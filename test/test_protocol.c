#include "unity/unity.h"
#include "../protocol.h"

#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 65535
static uint8_t buf[BUF_SIZE];

void setUp(void) {
    memset(buf, 0, BUF_SIZE);
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_join_message);
    return UNITY_END();
}
