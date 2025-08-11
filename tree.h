#ifndef TREE_H
#define TREE_H

typedef struct node_t node_t;

typedef struct tree_t {
    node_t *root;
    int size;
} tree_t;

extern void *no_node_sentinel;

tree_t *tree_new(void);

void tree_free(tree_t *tree, void (*free_value_func)(void *));

/**
 * Inserts the value and associates it with the key.
 *
 * If the key already existed, the value that was previously associated with the
 * key is returned.
 *
 * If the key did not exist, `no_node_sentinel` is returned.
 */
void *tree_insert(tree_t *tree, int key, void *value);

/**
 * Returns the value for the given key or `no_node_sentinel` if the key did not
 * exist in the tree.
 */
void *tree_get(tree_t *tree, int key);

/**
 * Removes the node with the given key from the tree.
 *
 * Returns the value of the key or `no_node_sentinel` if the key did not exist
 * in the tree.
 */
void *tree_remove(tree_t *tree, int key);

/**
 * Clears all nodes from the tree, freeing values with `value_free_func`.
 */
void tree_clear(tree_t *tree, void (*value_free_func)(void *));

void tree_for_each_value(tree_t *tree, void (*func)(void *));

void tree_print(tree_t *tree);

#endif // TREE_H
