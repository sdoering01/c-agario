#include "tree.h"


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define LEFT 1
#define RIGHT 0

node_t *node_new(int key, void *value) {
    node_t *node = calloc(1, sizeof(node_t));
    // TODO: Handle allocation failure
    node->key = key;
    node->value = value;
    return node;
}

void *node_insert(node_t *node, int key, void *value) {
    assert(node);

    if (key == node->key) {
        void* prev_value = node->value;
        node->value = value;
        return prev_value;
    }

    // TODO: With the current implementation, replacing a node that holds `NULL`
    // as a value, will still update the balance factor, since a previous value
    // of `NULL` and the absence of a previous value (also `NULL`) cannot be
    // differentiated. This could be fixed with a sentinel value, but that would
    // make the API non-intuitive. Still, the caller might want to know whether
    // there was a node that held `NULL` or whether there was no node
    // previously.

    // TODO: Re-balance tree
    if (key < node->key) {
        if (node->left) {
            void *prev_value = node_insert(node->left, key, value);
            if (!prev_value) {
                node->balance_factor -= 1;
            }
            return prev_value;
        } else {
            node_t *new_node = node_new(key, value);
            node->left = new_node;
            node->balance_factor -= 1;
        }
    } else {
        if (node->right) {
            void *prev_value = node_insert(node->right, key, value);
            if (!prev_value) {
                node->balance_factor += 1;
            }
            return prev_value;
        } else {
            node_t *new_node = node_new(key, value);
            node->right = new_node;
            node->balance_factor += 1;
        }
    }

    return NULL;
}

void *node_get(node_t *node, int key) {
    if (!node) {
        return NULL;
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
    node_t *node = *node_addr;

    if (!node) {
        return NULL;
    }

    // TODO: Consider `NULL` values for nodes. See comment in `node_insert`.

    // TODO: Re-balance tree
    if (key == node->key) {
        void *value = node->value;

        _node_replace(node_addr);

        free(node);

        return value;
    } else if (key < node->key) {
        node_t *prev_value = node_remove(&node->left, key);
        if (prev_value) {
            node->balance_factor += 1;
        }
        return prev_value;
    } else {
        node_t *prev_value = node_remove(&node->right, key);
        if (prev_value) {
            node->balance_factor -= 1;
        }
        return prev_value;
    }
}


node_t **_node_find_deepest_child_in_direction(node_t **node_addr, int direction) {
    // if `go_left` is false, this means to go right

    node_t *node = *node_addr;
    assert(node);

    if (direction == LEFT && node->left) {
        node->balance_factor += 1;
        return _node_find_deepest_child_in_direction(&node->left, direction);
    } else if (direction == RIGHT && node->right) {
        node->balance_factor -= 1;
        return _node_find_deepest_child_in_direction(&node->right, direction);
    } else {
        return node_addr;
    }
}

void _node_replace(node_t **node_addr) {
    node_t *node = *node_addr;

    if (!node) {
        return;
    }

    // TODO: Update balance factor
    // TODO: Re-balance tree

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
        replacement_addr = _node_find_deepest_child_in_direction(child_addr, !child_direction);

        node_t *replacement_node = *replacement_addr;

        _node_replace(replacement_addr);

        *node_addr = replacement_node;
        replacement_node->left = node->left;
        replacement_node->right = node->right;
        replacement_node->balance_factor = node->balance_factor;

        if (child_direction == LEFT) {
            replacement_node->balance_factor += 1;
        } else {
            replacement_node->balance_factor -= 1;
        }
    } else {
        // Node has no children, so we just remove it. The calling function has
        // a pointer to it and ensures to free it.
        *node_addr = NULL;
    }
}

void _node_print_recursive(node_t *node, int depth) {
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

    _node_print_recursive(node->left, depth + 1);
    _node_print_recursive(node->right, depth + 1);

    for (int i = 0; i < depth; i++) {
        printf("%s", padding);
    }
    printf(")\n");
}

void node_print(node_t *node) {
    _node_print_recursive(node, 0);
}
