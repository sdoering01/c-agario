#include "tree.h"

#include <stdio.h>

int main(void) {
    node_t *root = NULL;

    // long nums[] = {5, 2, 3, 9, 10, 11, 4, 7, 6, 8};
    long nums[] = {1, 2, -1, 42, 1337, 37};

    int n_nums = sizeof(nums) / sizeof(nums[0]);
    for (int i = 0; i < n_nums; i++) {
        if (!root) {
            root = node_new(nums[i], (void *)nums[i]);
        } else {
            node_insert(&root, nums[i], (void *)nums[i]);
        }
        node_print(root);
        printf("--------------------\n");
    }

    printf("\nREMOVE\n\n");

    for (int i = 0; i < n_nums; i++) {
        node_remove(&root, nums[i]);
        printf("removed\n");
        node_print(root);
        printf("--------------------\n");
    }

    // node_insert(&root, 12, (void *)12);

    // node_print(root);

    // node_remove(&root, 9);
    //
    // node_print(root);

    return 0;
}
