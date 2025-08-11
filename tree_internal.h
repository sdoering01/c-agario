#ifndef TREE_INTERNAL_H
#define TREE_INTERNAL_H

#include "tree.h"

typedef struct node_t {
    int balance_factor;
    int key;
    void *value;
    struct node_t *left;
    struct node_t *right;
} node_t;

#endif // TREE_INTERNAL_H
