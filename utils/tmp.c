#include <stdio.h>
#include "d_array.h"

int main() {
    int *arr = NULL;
    da_init(arr);
    
    da_append(arr, 14);
    da_append(arr, 14);

    printf("arr[0]: %d", arr[0]);
    printf("arr[1]: %d", arr[1]);

    da_free(arr);
    return 0;
}
