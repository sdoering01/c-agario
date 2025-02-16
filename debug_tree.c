#include "tree.h"

int main(void) {
    node_t *root = node_new(5, (void *)5);

    node_insert(root, 2, (void *)2);
    node_insert(root, 3, (void *)3);
    node_insert(root, 9, (void *)9);
    node_insert(root, 10, (void *)10);
    node_insert(root, 11, (void *)11);
    node_insert(root, 4, (void *)4);
    node_insert(root, 7, (void *)7);
    node_insert(root, 6, (void *)6);
    node_insert(root, 8, (void *)8);

    node_print(root);

    node_remove(&root, 9);

    node_print(root);

    return 0;
}
