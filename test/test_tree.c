#include "unity/unity.h"
#include "../tree_internal.h"

#include <stdio.h>
#include <stdlib.h>

#define MSG_LEN 1024
static char msg[MSG_LEN];

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

int node_height(node_t *node) {
    if (!node) {
        return 0;
    } else {
        int left_height = node_height(node->left);
        int right_height = node_height(node->right);

        int greater_height = left_height < right_height ? right_height : left_height;
        return 1 + greater_height;
    }
}

void assert_correctly_balanced(node_t *node) {
    TEST_ASSERT(node->balance_factor >= -1 && node->balance_factor <= 1);

    snprintf(msg, MSG_LEN, "balance factor for node with value %p", node->value);

    int expected_balance_factor = node_height(node->right) - node_height(node->left);
    TEST_ASSERT_EQUAL_MESSAGE(expected_balance_factor, node->balance_factor, msg);

    if (node->left) {
        assert_correctly_balanced(node->left);
    }
    if (node->right) {
        assert_correctly_balanced(node->right);
    }
}

void assert_integrity(tree_t *tree) {
    if (tree->root) {
        assert_keys_ordered(tree->root);
        assert_correctly_balanced(tree->root);
    }
}

void test_tree_insert(void) {
    tree_t *tree = tree_new();
    tree_insert(tree, 1, (void *)1);

    TEST_ASSERT_EQUAL(tree_get(tree, 2), no_node_sentinel);

    TEST_ASSERT_EQUAL(tree_insert(tree, 2, (void *)2), no_node_sentinel);
    TEST_ASSERT_EQUAL(tree_insert(tree, -1, (void *)-1), no_node_sentinel);
    TEST_ASSERT_EQUAL(tree_insert(tree, 42, (void *)42), no_node_sentinel);
    TEST_ASSERT_EQUAL(tree_insert(tree, 1337, (void *)1337), no_node_sentinel);
    TEST_ASSERT_EQUAL(tree_insert(tree, 37, (void *)37), no_node_sentinel);

    TEST_ASSERT_EQUAL(2, (long)tree_get(tree, 2));
    TEST_ASSERT_EQUAL(-1, (long)tree_get(tree, -1));
    TEST_ASSERT_EQUAL(42, (long)tree_get(tree, 42));
    TEST_ASSERT_EQUAL(1337, (long)tree_get(tree, 1337));
    TEST_ASSERT_EQUAL(37, (long)tree_get(tree, 37));

    TEST_ASSERT_EQUAL(tree_get(tree, 21), no_node_sentinel);

    TEST_ASSERT_EQUAL(2, (long)tree_insert(tree, 2, (void *)2));
    TEST_ASSERT_EQUAL(2, (long)tree_get(tree, 2));

    assert_integrity(tree);
}

void test_node_removal(void) {
    tree_t *tree = tree_new();
    tree_insert(tree, 5, (void *)5);

    long keys[] = {2, 3, 9, 10, 11, 4, 7, 6, 8};
    int num_keys = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < num_keys; i++) {
        long value = 2 * keys[i];
        tree_insert(tree, keys[i], (void *)value);
    }

    assert_integrity(tree);

    for (int i = 0; i < num_keys; i++) {
        long value = 2 * keys[i];
        TEST_ASSERT_EQUAL(value, tree_remove(tree, keys[i]));

        assert_integrity(tree);
        TEST_ASSERT_EQUAL(tree_get(tree, keys[i]), no_node_sentinel);
        TEST_ASSERT_EQUAL(tree_remove(tree, keys[i]), no_node_sentinel);
    }

}

void test_with_random_numbers(void) {
    srand(42);

    const int max_nodes = 100;
    int keys[100] = {0};

    for (int n_nodes = 1; n_nodes < max_nodes; n_nodes++) {
        tree_t *tree = tree_new();

        for (int i = 0; i < n_nodes; i++) {
            long key = rand();
            void *value = (void *)(2 * key);

            keys[i] = key;

            TEST_ASSERT_EQUAL(no_node_sentinel, tree_insert(tree, key, value));
            TEST_ASSERT_EQUAL(value, tree_insert(tree, key, value));

            TEST_ASSERT_EQUAL(i + 1, tree->size);

            assert_integrity(tree);
        }

        for (int i = 0; i < n_nodes; i++) {
            long key = keys[i];
            long value = 2 * key;

            TEST_ASSERT_EQUAL(value, tree_remove(tree, key));
            TEST_ASSERT_EQUAL(no_node_sentinel, tree_remove(tree, key));
            TEST_ASSERT_EQUAL(tree_get(tree, key), no_node_sentinel);

            TEST_ASSERT_EQUAL(n_nodes - i - 1, tree->size);

            assert_integrity(tree);
        }
    }
}

void test_tree_free_with_no_free_func(void) {
    tree_t *tree = tree_new();
    tree_insert(tree, 0, (void *)8);
    tree_insert(tree, 2, (void *)9);
    tree_insert(tree, -8, (void *)17);
    tree_insert(tree, 3, (void *)-3);

    tree_free(tree, NULL);
}

int tree_free_sum = 0;

void value_free_func(void *val) {
    tree_free_sum += (long)val;
}

void test_tree_free(void) {
    tree_t *tree = tree_new();
    tree_insert(tree, 0, (void *)8);
    tree_insert(tree, 2, (void *)9);
    tree_insert(tree, -8, (void *)17);
    tree_insert(tree, 3, (void *)-3);

    tree_free(tree, value_free_func);

    TEST_ASSERT_EQUAL(31, tree_free_sum);
}

int tree_for_each_value_sum = 0;

void for_each_value(void *val) {
    tree_for_each_value_sum += (long)val;
}

void test_tree_for_each_value(void) {
    tree_t *tree = tree_new();
    tree_insert(tree, 7, (void *)0);
    tree_insert(tree, 1, (void *)5);
    tree_insert(tree, 0, (void *)2);
    tree_insert(tree, -3, (void *)-8);
    tree_insert(tree, 12, (void *)4);

    tree_for_each_value(tree, for_each_value);

    TEST_ASSERT_EQUAL(3, tree_for_each_value_sum);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tree_insert);
    RUN_TEST(test_node_removal);
    RUN_TEST(test_with_random_numbers);
    RUN_TEST(test_tree_free_with_no_free_func);
    RUN_TEST(test_tree_free);
    RUN_TEST(test_tree_for_each_value);
    return UNITY_END();
}
