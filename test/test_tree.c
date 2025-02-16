#include "unity/unity.h"
#include "../tree.h"

#include <stdlib.h>

void setUp(void) {
}

void tearDown(void) {
}

void assert_keys_ordered(node_t *node) {
    if (node->left) {
        TEST_ASSERT_LESS_THAN(node->key, node->left->key);
        assert_keys_ordered(node->left);
    }
    if (node->right) {
        TEST_ASSERT_GREATER_THAN(node->key, node->right->key);
        assert_keys_ordered(node->right);
    }
}

int count_nodes(node_t *node) {
    if (!node) {
        return 0;
    } else {
        return 1 + count_nodes(node->left) + count_nodes(node->right);
    }
}

void assert_correctly_balanced(node_t *node) {
    // TODO: Add check whether balance_factor is in interval [-1,1]

    int expected_balance_factor = count_nodes(node->right) - count_nodes(node->left);
    TEST_ASSERT_EQUAL(expected_balance_factor, node->balance_factor);

    if (node->left) {
        assert_correctly_balanced(node->left);
    }
    if (node->right) {
        assert_correctly_balanced(node->right);
    }
}

void assert_integrity(node_t *node) {
    assert_keys_ordered(node);
    assert_correctly_balanced(node);
}

void test_node_insert(void) {
    node_t *root = node_new(1, (void *)1);

    TEST_ASSERT_NULL(node_get(root, 2));

    TEST_ASSERT_NULL(node_insert(root, 2, (void *)2));
    TEST_ASSERT_NULL(node_insert(root, -1, (void *)-1));
    TEST_ASSERT_NULL(node_insert(root, 42, (void *)42));

    TEST_ASSERT_EQUAL(2, (long)node_get(root, 2));
    TEST_ASSERT_EQUAL(-1, (long)node_get(root, -1));
    TEST_ASSERT_EQUAL(42, (long)node_get(root, 42));

    TEST_ASSERT_NULL(node_get(root, 21));

    TEST_ASSERT_EQUAL(2, (long)node_insert(root, 2, (void *)2));
    TEST_ASSERT_EQUAL(2, (long)node_get(root, 2));

    assert_integrity(root);
}

void test_node_removal(void) {
    node_t *root = node_new(5, (void *)5);

    long keys[] = {2, 3, 9, 10, 11, 4, 7, 6, 8};
    int num_keys = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < num_keys; i++) {
        long value = 2 * keys[i];
        node_insert(root, keys[i], (void *)value);
    }

    assert_integrity(root);

    for (int i = 0; i < num_keys; i++) {
        long value = 2 * keys[i];
        TEST_ASSERT_EQUAL(value, node_remove(&root, keys[i]));

        assert_integrity(root);
        TEST_ASSERT_NULL(node_get(root, keys[i]));
        TEST_ASSERT_NULL(node_remove(&root, keys[i]));
    }

}

void test_with_random_numbers(void) {
    srand(42);

    const int max_nodes = 100;
    int keys[100] = {0};

    for (int n_nodes = 1; n_nodes < max_nodes; n_nodes++) {
        node_t *root = NULL;

        for (int i = 0; i < n_nodes; i++) {
            long key = rand();
            void *value = (void *)(2 * key);

            keys[i] = key;

            if (root == NULL) {
                root = node_new(key, value);
            } else {
                node_insert(root, key, value);
            }

            assert_integrity(root);
        }

        for (int i = 0; i < n_nodes; i++) {
            long key = keys[i];
            long value = 2 * key;

            TEST_ASSERT_EQUAL(value, node_remove(&root, key));
            TEST_ASSERT_NULL(node_get(root, key));

            if (root) {
                assert_integrity(root);
            }
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_node_insert);
    RUN_TEST(test_node_removal);
    RUN_TEST(test_with_random_numbers);
    return UNITY_END();
}
