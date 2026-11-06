/*
 * File: logger_thread.c
 * Purpose: Logger thread for Sensor Monitoring System.
 * Author: Student
 * Student ID: SE204292
 * Date: 12/11/2026
 */

#include "sms_app.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_LOG_SIZE (1024 * 1024)
#define FLUSH_INTERVAL_SEC 5

static FILE *log_file = NULL;

/* Mo file log */
static int open_log_file(void)
{
    log_file = fopen(LOG_FILE, "a");

    if (log_file == NULL) {
        perror("Cannot open log file");
        return -1;
    }

    return 0;
}

/* Kiem tra va xoay vong file log */
static int rotate_log_file(void)
{
    struct stat file_stat;

    if (log_file == NULL)
        return -1;

    fflush(log_file);

    if (stat(LOG_FILE, &file_stat) == 0 &&
        file_stat.st_size >= MAX_LOG_SIZE) {

        fclose(log_file);
        log_file = NULL;

        unlink(LOG_FILE ".1");

        if (rename(LOG_FILE, LOG_FILE ".1") != 0)
            perror("Cannot rotate log file");

        return open_log_file();
    }

    return 0;
}
/* Logger Thread */
/* Logger Thread */
void *logger_thread(void *arg)
{
    LogMessage message;
    time_t last_flush = time(NULL);

    (void)arg;

    if (open_log_file() != 0)
        return NULL;

while (1) {
    ssize_t received = mq_receive(
        log_queue, (char *)&message, sizeof(message), NULL);

    if (received >= 0) {

        /* Nhan tin hieu dung tu Main Thread */
        if (message.message[0] == '\0')
            break;

        time_t now = time(NULL);
        struct tm time_info;
        char timestamp[32];

        localtime_r(&now, &time_info);

        strftime(timestamp, sizeof(timestamp),
                 "%Y-%m-%d %H:%M:%S", &time_info);

        fprintf(log_file, "[%s] %s\n",
                timestamp, message.message);

        if (rotate_log_file() != 0)
            break;
    } else {
        if (errno == EAGAIN || errno == EINTR) {
            usleep(100000);
        } else {
            perror("mq_receive");
            break;
        }
    }

    if (time(NULL) - last_flush >= FLUSH_INTERVAL_SEC) {
        fflush(log_file);
        last_flush = time(NULL);
    }
}
    if (log_file != NULL) {
        fflush(log_file);
        fclose(log_file);
        log_file = NULL;
    }
    return NULL;
}
