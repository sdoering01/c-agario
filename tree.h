#ifndef TREE_H
#define TREE_H

/**
 * Currently, no values of `NULL` should be stored in the nodes, since this case
 * is not handled yet, and will cause the tree to become imbalanced.
 */
typedef struct node_t {
    int balance_factor;
    int key;
    void *value;
    struct node_t *left;
    struct node_t *right;
} node_t;

node_t *node_new(int key, void *value);

extern void *no_node_sentinel;

/**
 * Inserts the value and associates it with the key.
 *
 * If the key already existed, the value that was previously associated with the
 * key is returned.
 */
void *node_insert(node_t **node, int key, void *value);

void *node_get(node_t *node, int key);

// TODO: Maybe make this private
void _node_replace(node_t **node_addr);

void _node_rebalance(node_t **node_addr);

void *node_remove(node_t **node_addr, int key);

void node_print(node_t *node);

#endif // TREE_H
