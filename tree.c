#include "tree_internal.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdalign.h>

#define LEFT 1
#define RIGHT 0

static alignas(max_align_t) char no_node_sentinel_data;
void *no_node_sentinel = &no_node_sentinel_data;

/**
 * Iniitial left rotation in large rotation.
 */
static void node_rotate_left_prepare(node_t **node_addr) {
    node_t *old_parent = *node_addr;
    node_t *new_parent = old_parent->right;
    *node_addr = new_parent;
    old_parent->right = new_parent->left;
    new_parent->left = old_parent;

    if (new_parent->balance_factor == 0) {
        new_parent->balance_factor = -1;
        old_parent->balance_factor = 0;
    } else if (new_parent->balance_factor == 1) {
        new_parent->balance_factor = -1;
        old_parent->balance_factor = -1;
    } else if (new_parent->balance_factor == -1) {
        new_parent->balance_factor = -2;
        old_parent->balance_factor = 0;
    }
}

/**
 * Iniitial right rotation in large rotation.
 */
static void node_rotate_right_prepare(node_t **node_addr) {
    node_t *old_parent = *node_addr;
    node_t *new_parent = old_parent->left;
    *node_addr = new_parent;
    old_parent->left = new_parent->right;
    new_parent->right = old_parent;

    if (new_parent->balance_factor == 0) {
        new_parent->balance_factor = 1;
        old_parent->balance_factor = 0;
    } else if (new_parent->balance_factor == -1) {
        new_parent->balance_factor = 1;
        old_parent->balance_factor = 1;
    } else if (new_parent->balance_factor == 1) {
        new_parent->balance_factor = 2;
        old_parent->balance_factor = 0;
    }
}

static void node_rotate_left(node_t **node_addr) {
    node_t *old_parent = *node_addr;
    node_t *new_parent = old_parent->right;
    *node_addr = new_parent;
    old_parent->right = new_parent->left;
    new_parent->left = old_parent;

    if (new_parent->balance_factor == 0) {
        old_parent->balance_factor = 1;
        new_parent->balance_factor = -1;
    } else if (new_parent->balance_factor == 2) {
        // Could extract this into function that does big rotation, since it is
        // only needed there
        old_parent->balance_factor = -1;
        new_parent->balance_factor = 0;
    } else {
        old_parent->balance_factor = 0;
        new_parent->balance_factor = 0;
    }
}

static void node_rotate_right(node_t **node_addr) {
    node_t *old_parent = *node_addr;
    node_t *new_parent = old_parent->left;
    *node_addr = new_parent;
    old_parent->left = new_parent->right;
    new_parent->right = old_parent;

    if (new_parent->balance_factor == 0) {
        old_parent->balance_factor = -1;
        new_parent->balance_factor = 1;
    } else if (new_parent->balance_factor == -2) {
        // Could extract this into function that does big rotation, since it is
        // only needed there
        old_parent->balance_factor = 1;
        new_parent->balance_factor = 0;
    } else {
        old_parent->balance_factor = 0;
        new_parent->balance_factor = 0;
    }
}

static void node_rebalance(node_t **node_addr) {
    node_t *node = *node_addr;

    if (node->balance_factor < -1) {
        if (node->left->balance_factor <= 0) {
            node_rotate_right(node_addr);
        } else {
            node_rotate_left_prepare(&node->left);
            node_rotate_right(node_addr);
        }
    } else if (node->balance_factor > 1) {
        if (node->right->balance_factor >= 0) {
            node_rotate_left(node_addr);
        } else {
            node_rotate_right_prepare(&node->right);
            node_rotate_left(node_addr);
        }
    }
}

static node_t **node_find_deepest_child_in_direction(node_t **node_addr, int direction) {
    node_t *node = *node_addr;
    node_t **ret_node_addr = node_addr;
    assert(node);

    if (direction == LEFT && node->left) {
        int prev_left_balance_factor = node->left->balance_factor;
        ret_node_addr = node_find_deepest_child_in_direction(&node->left, direction);
        // `node->left` is the last node in this direction, so we will take it out and thus shorten the left sub-tree
        if (!node->left->left || (prev_left_balance_factor != 0 && node->left->balance_factor == 0)) {
            node->balance_factor += 1;
            node_rebalance(node_addr);
        }
    } else if (direction == RIGHT && node->right) {
        int prev_right_balance_factor = node->right->balance_factor;
        ret_node_addr = node_find_deepest_child_in_direction(&node->right, direction);
        if (!node->right->right || (prev_right_balance_factor != 0 && node->right->balance_factor == 0)) {
            node->balance_factor -= 1;
            node_rebalance(node_addr);
        }
    } else {
        ret_node_addr = node_addr;
    }

    return ret_node_addr;
}

/**
 * Finds a replacement (or NULL) for the node at `node_addr` and assigns it to
 * that address.
 */
static void node_replace(node_t **node_addr) {
    node_t *node = *node_addr;

    if (!node) {
        return;
    }

    if (node->left || node->right) {
        node_t **replacement_addr = NULL;
        // IMPORTANT: Make sure to not lose access to the node's children when replacing them.

        int child_direction;

        if (node->left && node->right) {
            if (node->balance_factor <= 0) {
                child_direction = LEFT;
            } else {
                child_direction = RIGHT;
            }
        } else if (node->left) {
            child_direction = LEFT;
        } else {
            child_direction = RIGHT;
        }

        node_t **child_addr = child_direction == LEFT ? &node->left : &node->right;
        int prev_child_balance_factor = (*child_addr)->balance_factor;
        replacement_addr = node_find_deepest_child_in_direction(child_addr, !child_direction);

        node_t *replacement_node = *replacement_addr;

        node_replace(replacement_addr);

        *node_addr = replacement_node;
        replacement_node->left = node->left;
        replacement_node->right = node->right;
        replacement_node->balance_factor = node->balance_factor;

        if (child_direction == LEFT) {
            if (!node->left || (prev_child_balance_factor != 0 && node->left->balance_factor == 0)) {
                replacement_node->balance_factor += 1;
                node_rebalance(node_addr);
            }
        } else {
            if (!node->right || (prev_child_balance_factor != 0 && node->right->balance_factor == 0)) {
                replacement_node->balance_factor -= 1;
                node_rebalance(node_addr);
            }
        }
    } else {
        // Node has no children, so we just remove it. The calling function has
        // a pointer to it and ensures to free it.
        *node_addr = NULL;
    }
}

static node_t *node_new(int key, void *value) {
    node_t *node = calloc(1, sizeof(node_t));
    // TODO: Handle allocation failure
    node->key = key;
    node->value = value;
    return node;
}

static inline void node_free(node_t *node, void (*value_free_func)(void *)) {
    if (value_free_func) {
        value_free_func(node->value);
    }
    free(node);
}

void *node_insert(node_t **node_addr, int key, void *value) {
    void *return_value = no_node_sentinel;
    node_t *node = *node_addr;

    assert(node);

    if (key == node->key) {
        void* prev_value = node->value;
        node->value = value;
        return prev_value;
    }

    if (key < node->key) {
        if (node->left) {
            int prev_balance_factor = node->left->balance_factor;
            return_value = node_insert(&node->left, key, value);
            int new_balance_factor = node->left->balance_factor;
            if (prev_balance_factor == 0 && new_balance_factor != 0) {
                node->balance_factor -= 1;
            }
        } else {
            node_t *new_node = node_new(key, value);
            node->left = new_node;
            node->balance_factor -= 1;
        }
    } else {
        if (node->right) {
            int prev_balance_factor = node->right->balance_factor;
            return_value = node_insert(&node->right, key, value);
            int new_balance_factor = node->right->balance_factor;
            if (prev_balance_factor == 0 && new_balance_factor != 0) {
                node->balance_factor += 1;
            }
        } else {
            node_t *new_node = node_new(key, value);
            node->right = new_node;
            node->balance_factor += 1;
        }
    }

    node_rebalance(node_addr);

    return return_value;
}

void *node_get(node_t *node, int key) {
    if (!node) {
        return no_node_sentinel;
    }

    if (key == node->key) {
        return node->value;
    } else if (key < node->key) {
        return node_get(node->left, key);
    } else {
        return node_get(node->right, key);
    }
}

void *node_remove(node_t **node_addr, int key) {
    void *return_value;
    node_t *node = *node_addr;

    if (!node) {
        return no_node_sentinel;
    }

    if (key == node->key) {
        void *value = node->value;

        node_replace(node_addr);

        free(node);

        return_value = value;
    } else if (key < node->key) {
        int prev_left_balance_factor = node->left ? node->left->balance_factor : 0;
        node_t *prev_value = node_remove(&node->left, key);
        if (prev_value != no_node_sentinel) {
            if (!node->left || (prev_left_balance_factor != 0 && node->left->balance_factor == 0)) {
                node->balance_factor += 1;
                node_rebalance(node_addr);
            }
        }
        return_value = prev_value;
    } else {
        int prev_right_balance_factor = node->right ? node->right->balance_factor : 0;
        node_t *prev_value = node_remove(&node->right, key);
        if (prev_value != no_node_sentinel) {
            if (!node->right || (prev_right_balance_factor != 0 && node->right->balance_factor == 0)) {
                node->balance_factor -= 1;
                node_rebalance(node_addr);
            }
        }
        return_value = prev_value;
    }

    return return_value;
}

void node_print_recursive(node_t *node, int depth) {
    const char *padding = "    ";

    for (int i = 0; i < depth; i++) {
        printf("%s", padding);
    }

    if (node) {
        printf("%d [bf=%d] (\n", node->key, node->balance_factor);
    } else {
        printf("<null>\n");
        return;
    }

    node_print_recursive(node->left, depth + 1);
    node_print_recursive(node->right, depth + 1);

    for (int i = 0; i < depth; i++) {
        printf("%s", padding);
    }
    printf(")\n");
}

void node_free_recursive(node_t *node, void (*value_free_func)(void *)) {
    if (node) {
        node_free_recursive(node->left, value_free_func);
        node_free_recursive(node->right, value_free_func);
        node_free(node, value_free_func);
    }
}

void node_for_each_recursive(node_t *node, void (*func)(void *)) {
    if (node) {
        func(node->value);
        node_for_each_recursive(node->left, func);
        node_for_each_recursive(node->right, func);
    }
}

tree_t *tree_new(void) {
    tree_t *tree = calloc(1, sizeof(tree_t));
    // TODO: Handle allocation failure
    return tree;
}

void tree_free(tree_t *tree, void (*value_free_func)(void *)) {
    node_free_recursive(tree->root, value_free_func);
    free(tree);
}

void *tree_insert(tree_t *tree, int key, void *value) {
    assert(tree);

    if (tree->root) {
        void *ret_value = node_insert(&tree->root, key, value);
        if (ret_value == no_node_sentinel) {
            tree->size += 1;
        }
        return ret_value;
    } else {
        tree->root = node_new(key, value);
        tree->size = 1;
        return no_node_sentinel;
    }
}

void *tree_get(tree_t *tree, int key) {
    assert(tree);
    return node_get(tree->root, key);
}

void *tree_remove(tree_t *tree, int key) {
    assert(tree);

    void *ret_value = node_remove(&tree->root, key);
    if (ret_value != no_node_sentinel) {
        tree->size -= 1;
    }
    return ret_value;
}

void tree_clear(tree_t *tree, void (*value_free_func)(void *)) {
    node_free_recursive(tree->root, value_free_func);
    tree->root = NULL;
    tree->size = 0;
}

void tree_for_each_value(tree_t *tree, void (*func)(void *)) {
    node_for_each_recursive(tree->root, func);
}

void tree_print(tree_t *tree) {
    assert(tree);
    node_print_recursive(tree->root, 0);
}
