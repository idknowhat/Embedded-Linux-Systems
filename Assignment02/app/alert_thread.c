/*
 * File: alert_thread.c
 * Purpose: Alert thread for Sensor Monitoring System.
 * Author: Student
 * Student ID: SE204292
 * Date: 12/11/2026
 */

#include "sms_app.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

/* In thong tin canh bao */
static void print_alert(const AlertMessage *alert)
{
    const char *type_name;

    switch (alert->type) {
    case ALERT_HIGH_TEMPERATURE:
        type_name = "HIGH_TEMPERATURE";
        break;

    case ALERT_HIGH_HUMIDITY:
        type_name = "HIGH_HUMIDITY";
        break;

    case ALERT_TEMP_CHANGE:
        type_name = "TEMP_CHANGE";
        break;

    default:
        type_name = "UNKNOWN";
        break;
    }

    fprintf(stderr,
            "[ALERT][%llu] TYPE: %s VALUE: %.2f THRESHOLD: %.2f\n",
            (unsigned long long)alert->timestamp,
            type_name,
            alert->value,
            alert->threshold);
}

/* Alert Thread */
void *alert_thread(void *arg)
{
    AlertMessage alert;

    unsigned long temp_count = 0;
    unsigned long humidity_count = 0;
    unsigned long change_count = 0;

    (void)arg;

while (1) {
    ssize_t received = mq_receive(
        alarm_queue, (char *)&alert, sizeof(alert), NULL);

    if (received >= 0) {

        /* Nhan tin hieu dung tu Main Thread */
        if (alert.timestamp == 0)
            break;

        switch (alert.type) {
        case ALERT_HIGH_TEMPERATURE:
            temp_count++;
            break;

        case ALERT_HIGH_HUMIDITY:
            humidity_count++;
            break;

        case ALERT_TEMP_CHANGE:
            change_count++;
            break;

        default:
            break;
        }

        print_alert(&alert);

        char log_text[LOG_MSG_SIZE];

        snprintf(log_text, sizeof(log_text),
                 "ALERT: type=%d value=%.2f threshold=%.2f",
                 alert.type,
                 alert.value,
                 alert.threshold);

        send_log_message(log_text);
    } else {
        if (errno == EAGAIN) {
            usleep(1000);
        } else if (errno == EINTR) {
            continue;
        } else {
            perror("mq_receive");
            break;
        }
    }
}
    fprintf(stderr, "\nAlert Statistics:\n");
    fprintf(stderr, "High temperature : %lu\n", temp_count);
    fprintf(stderr, "High humidity    : %lu\n", humidity_count);
    fprintf(stderr, "Temperature change: %lu\n", change_count);

    return NULL;
}

