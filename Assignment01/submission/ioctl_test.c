#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "asgn1_ioctl.h"

#define DEVICE "/dev/asgn1"

int main(int argc, char *argv[])
{
    int fd;

    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s reset\n", argv[0]);
        printf("  %s stats\n", argv[0]);
        printf("  %s mode 0|1\n", argv[0]);
        printf("  %s version\n", argv[0]);
        printf("  %s write <text>\n", argv[0]);
        printf("  %s read\n", argv[0]);
        return 1;
    }

    fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (strcmp(argv[1], "reset") == 0) {
        if (ioctl(fd, ASGN1_RESET_BUFFER) < 0)
            perror("RESET_BUFFER");
        else
            printf("RESET_BUFFER: SUCCESS\n");
    }

    else if (strcmp(argv[1], "stats") == 0) {
        struct asgn1_stats stats;

        if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
            perror("GET_STATS");
        } else {
            printf("Driver statistics:\n");
            printf("open_count      : %d\n", stats.open_count);
            printf("write_count     : %d\n", stats.write_count);
            printf("read_count      : %d\n", stats.read_count);
            printf("buffer_len      : %d\n", stats.buffer_len);
            printf("last_write_size : %d\n", stats.last_write_size);
        }
    }

    else if (strcmp(argv[1], "mode") == 0 && argc >= 3) {
        int mode = atoi(argv[2]);

        if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0)
            perror("SET_MODE");
        else
            printf("SET_MODE: SUCCESS, mode=%d\n", mode);
    }

    else if (strcmp(argv[1], "version") == 0) {
        char version[64] = {0};

        if (ioctl(fd, ASGN1_GET_VERSION, version) < 0)
            perror("GET_VERSION");
        else
            printf("Driver version: %s\n", version);
    }

    else if (strcmp(argv[1], "write") == 0 && argc >= 3) {
        ssize_t n = write(fd, argv[2], strlen(argv[2]));

        if (n < 0)
            perror("write");
        else
            printf("WRITE: %zd bytes\n", n);
    }

    else if (strcmp(argv[1], "read") == 0) {
        char buffer[1024] = {0};
        ssize_t n = read(fd, buffer, sizeof(buffer) - 1);

        if (n < 0)
            perror("read");
        else {
            buffer[n] = '\0';
            printf("READ: %zd bytes\n", n);
            printf("Data: %s\n", buffer);
        }
    }

    else if (strcmp(argv[1], "invalid") == 0) {
        if (ioctl(fd, 0x12345678) < 0)
            perror("INVALID_IOCTL");
        else
            printf("INVALID_IOCTL: SUCCESS (unexpected)\n");
    }

    else {
        printf("Invalid command\n");
    }

    close(fd);
    return 0;
}
