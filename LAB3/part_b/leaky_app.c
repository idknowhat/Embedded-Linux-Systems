#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    printf("=== Memory Leak Test ===\n");

    char *data = malloc(100);

    if (data == NULL) {
        printf("Memory allocation failed!\n");
        return 1;
    }

    strcpy(data, "This memory is intentionally leaked!");
printf("Data: %s\n", data);

free(data);

return 0;
    printf("Data: %s\n", data);

    // Intentionally missing free(data)
    return 0;
}
