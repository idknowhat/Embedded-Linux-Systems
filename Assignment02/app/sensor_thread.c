/*
 * File: sensor_thread.c
 * Purpose: Read sensor data and measure real-time scheduling.
 * Author: Student
 * Student ID: SE204292
 * Date: 12/11/2026
 */

#include "sms_app.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* Sleep using absolute CLOCK_MONOTONIC time */
/* Sleep until an absolute deadline and measure wakeup jitter */
static long long sleep_until(struct timespec *deadline)
{
    int ret;
    struct timespec actual_time;

    do {
        ret = clock_nanosleep(
            CLOCK_MONOTONIC,
            TIMER_ABSTIME,
            deadline,
            NULL
        );
    } while (ret == EINTR && app_running);

    clock_gettime(CLOCK_MONOTONIC, &actual_time);

    long long expected_ns =
        (long long)deadline->tv_sec * 1000000000LL +
        deadline->tv_nsec;

    long long actual_ns =
        (long long)actual_time.tv_sec * 1000000000LL +
        actual_time.tv_nsec;

    return actual_ns - expected_ns;
}

#define JITTER_SAMPLE_COUNT 1000

static long long jitter_samples[JITTER_SAMPLE_COUNT];
static unsigned int jitter_count = 0;

/* Compare function for qsort */
static int compare_jitter(const void *a, const void *b)
{
    long long x = *(const long long *)a;
    long long y = *(const long long *)b;

    if (x < y)
        return -1;
    if (x > y)
        return 1;

    return 0;
}

/* Store jitter samples and print statistics after 1000 samples */
static void record_jitter(long long jitter_ns)
{
    if (jitter_count >= JITTER_SAMPLE_COUNT)
        return;

    jitter_samples[jitter_count++] = jitter_ns;

    if (jitter_count < JITTER_SAMPLE_COUNT)
        return;

    long long sorted[JITTER_SAMPLE_COUNT];
    long long min = jitter_samples[0];
    long long max = jitter_samples[0];
    long long sum = 0;

    unsigned int histogram[6] = {0};

    memcpy(sorted, jitter_samples, sizeof(sorted));

    for (unsigned int i = 0; i < JITTER_SAMPLE_COUNT; i++) {
        if (jitter_samples[i] < min)
            min = jitter_samples[i];

        if (jitter_samples[i] > max)
            max = jitter_samples[i];

        sum += jitter_samples[i];

        double us = jitter_samples[i] / 1000.0;

        if (us <= 100)
            histogram[0]++;
        else if (us <= 250)
            histogram[1]++;
        else if (us <= 500)
            histogram[2]++;
        else if (us <= 1000)
            histogram[3]++;
        else if (us <= 2000)
            histogram[4]++;
        else
            histogram[5]++;
    }

    qsort(sorted, JITTER_SAMPLE_COUNT,
          sizeof(long long), compare_jitter);

    double average = (double)sum / JITTER_SAMPLE_COUNT;
    long long p99 = sorted[989];

    const char *filename =
        app_config.sched_policy == SCHED_FIFO
        ? "/tmp/jitter_fifo.csv"
        : "/tmp/jitter_other.csv";

    FILE *fp = fopen(filename, "w");

    if (fp != NULL) {
        fprintf(fp, "sample,jitter_ns,jitter_us\n");

        for (unsigned int i = 0;
             i < JITTER_SAMPLE_COUNT; i++) {
            fprintf(fp, "%u,%lld,%.3f\n",
                    i + 1,
                    jitter_samples[i],
                    jitter_samples[i] / 1000.0);
        }

        fclose(fp);
    } else {
        perror("Cannot create jitter CSV");
    }

    printf("\n========== JITTER RESULTS ==========\n");
    printf("Scheduling policy : %s\n",
           app_config.sched_policy == SCHED_FIFO
           ? "SCHED_FIFO" : "SCHED_OTHER");
    printf("Samples           : %u\n", jitter_count);
    printf("Minimum           : %.3f us\n", min / 1000.0);
    printf("Average           : %.3f us\n", average / 1000.0);
    printf("Maximum           : %.3f us\n", max / 1000.0);
    printf("P99               : %.3f us\n", p99 / 1000.0);

    printf("\nJitter Histogram:\n");
    printf("<= 100 us   : %u\n", histogram[0]);
    printf("100-250 us  : %u\n", histogram[1]);
    printf("250-500 us  : %u\n", histogram[2]);
    printf("500-1000 us : %u\n", histogram[3]);
    printf("1-2 ms      : %u\n", histogram[4]);
    printf("> 2 ms      : %u\n", histogram[5]);

    printf("CSV saved to: %s\n", filename);
    printf("====================================\n\n");
}
void *sensor_thread(void *arg)
{
    char read_buffer[128];

    (void)arg;

    /* Configure real-time scheduling policy */
    struct sched_param sched_param;
    memset(&sched_param, 0, sizeof(sched_param));

    if (app_config.sched_policy == SCHED_FIFO) {
        sched_param.sched_priority = 50;
    } else {
        sched_param.sched_priority = 0;
    }

    int sched_ret = pthread_setschedparam(
        pthread_self(),
        app_config.sched_policy,
        &sched_param
    );

    if (sched_ret != 0) {
        fprintf(stderr,
                "Cannot set scheduling policy: %s\n",
                strerror(sched_ret));
    } else {
        printf("[REAL-TIME] Scheduling policy: %s, priority: %d\n",
               app_config.sched_policy == SCHED_FIFO
                   ? "SCHED_FIFO" : "SCHED_OTHER",
               sched_param.sched_priority);
    }

    struct timespec next_time;

    while (app_running) {

        int fd = open(SENSOR_DEVICE, O_RDONLY);

        if (fd < 0) {
            perror("Cannot open sensor device");
            app_running = 0;
            break;
        }

        /* Configure sampling interval through kernel IOCTL */
        if (ioctl(fd, SMS_SET_INTERVAL,
                  &app_config.interval_ms) < 0) {
            perror("SMS_SET_INTERVAL ioctl");
            close(fd);
            app_running = 0;
            break;
        }

        ssize_t bytes_read = read(
            fd,
            read_buffer,
            sizeof(read_buffer) - 1
        );

        close(fd);

        if (bytes_read > 0) {

            read_buffer[bytes_read] = '\0';

            SensorData data = {0};
            unsigned long long timestamp = 0;

            if (sscanf(read_buffer, "%llu,%f,%f",
                       &timestamp,
                       &data.temperature,
                       &data.humidity) == 3) {

                data.timestamp = (uint64_t)timestamp;

                if (!buffer_push(&sensor_buffer, data)) {
                    send_log_message(
                        "WARNING: Circular buffer full, sample dropped"
                    );
                }

            } else {
                fprintf(stderr,
                        "Invalid sensor data: %s\n",
                        read_buffer);
            }

        } else if (bytes_read < 0) {
            perror("Sensor read");
            app_running = 0;
            break;
        } else {
            fprintf(stderr, "Sensor device returned EOF\n");
            app_running = 0;
            break;
        }
        /* Wait for the next sampling interval and measure jitter */
        if (clock_gettime(CLOCK_MONOTONIC, &next_time) != 0) {
            perror("clock_gettime");
            app_running = 0;
            break;
        }

        next_time.tv_nsec +=
            (long)app_config.interval_ms * 1000000L;

        while (next_time.tv_nsec >= 1000000000L) {
            next_time.tv_nsec -= 1000000000L;
            next_time.tv_sec++;
        }

        record_jitter(sleep_until(&next_time));
    }

    return NULL;
}
