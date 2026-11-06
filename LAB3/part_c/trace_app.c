#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(void)
{
    printf("=== System Call Tracing Test ===\n");

    int fd = open("test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    char message[] = "Hello Embedded Linux!\n";

    write(fd, message, strlen(message));

    close(fd);

    printf("File written successfully!\n");

    return 0;
}
