#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#define SMS_MAGIC 'S'
#define SMS_SET_INTERVAL _IOW(SMS_MAGIC, 1, unsigned int)

int main(void)
{
    int fd;
    unsigned int interval = 2000;
    char buffer[128];
    ssize_t n;
    struct timespec start, end;

    /* Mo device va cai dat interval */
    fd = open("/dev/sms_sensor", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ioctl(fd, SMS_SET_INTERVAL, &interval) < 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }

    printf("Set sensor interval to %u ms\n", interval);

    /* Doc lan thu nhat */
    printf("Reading sensor first time...\n");

    n = read(fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        perror("first read");
        close(fd);
        return 1;
    }

    buffer[n] = '\0';
    printf("%s", buffer);

    close(fd);

    /* Bat dau do thoi gian */
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Mo lai device de doc lan thu hai */
    fd = open("/dev/sms_sensor", O_RDONLY);
    if (fd < 0) {
        perror("second open");
        return 1;
    }

    printf("Reading sensor second time...\n");

    n = read(fd, buffer, sizeof(buffer) - 1);

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (n <= 0) {
        perror("second read");
        close(fd);
        return 1;
    }

    buffer[n] = '\0';
    printf("%s", buffer);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("Elapsed time: %.3f seconds\n", elapsed);

    close(fd);

    return 0;
}
