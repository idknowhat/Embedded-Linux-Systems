/*
 * File: processor_thread.c
 * Purpose: Process sensor data and detect abnormal conditions.
 * Author: Student
 * Student ID: SE204292
 * Date: 12/11/2026
 */

#include "sms_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#define AVERAGE_WINDOW 10
#define TEMP_CHANGE_THRESHOLD 5.0f

static void send_alert(AlertType type,
                       const SensorData *data,
                       float threshold)
{
    AlertMessage alert;

    alert.timestamp = data->timestamp;
    alert.type = type;

    if (type == ALERT_HIGH_TEMPERATURE)
        alert.value = data->temperature;
    else if (type == ALERT_HIGH_HUMIDITY)
        alert.value = data->humidity;
    else
        alert.value = data->temperature;

    alert.threshold = threshold;
int ret;

do {
    ret = mq_send(alarm_queue,
                  (const char *)&alert,
                  sizeof(alert),
                  0);
} while (ret == -1 && errno == EINTR);

if (ret == -1) {
    if (errno == EAGAIN) {
        fprintf(stderr,
                "[WARNING] Alarm queue full, alert dropped.\n");
    } else {
        perror("Cannot send alert");
    }
}
}
static void send_processed_log(const SensorData *data,
                               float avg_temp,
                               float avg_humidity)
{
    char message[LOG_MSG_SIZE];

    snprintf(message, sizeof(message),
             "Sensor timestamp=%llu temp=%.2f humidity=%.2f "
             "avg_temp=%.2f avg_humidity=%.2f",
             (unsigned long long)data->timestamp,
             data->temperature,
             data->humidity,
             avg_temp,
             avg_humidity);

    send_log_message(message);
}

void *processor_thread(void *arg)
{
    SensorData data;

    float temp_history[AVERAGE_WINDOW] = {0};
    float humidity_history[AVERAGE_WINDOW] = {0};

    unsigned int history_count = 0;
    unsigned int history_index = 0;

    float previous_temperature = 0.0f;
    int has_previous = 0;

    (void)arg;

while (1) {

    if (!buffer_pop(&sensor_buffer, &data))
        continue;

    /* Nhan tin hieu dung tu Main Thread */
    if (data.timestamp == 0) {
        break;
    }

    temp_history[history_index] = data.temperature;
    humidity_history[history_index] = data.humidity;
        history_index = (history_index + 1) % AVERAGE_WINDOW;

        if (history_count < AVERAGE_WINDOW)
            history_count++;

        float temp_sum = 0.0f;
        float humidity_sum = 0.0f;

        for (unsigned int i = 0; i < history_count; i++) {
            temp_sum += temp_history[i];
            humidity_sum += humidity_history[i];
        }

        float avg_temp = temp_sum / history_count;
        float avg_humidity = humidity_sum / history_count;

        if (data.temperature > app_config.temp_threshold) {
            send_alert(ALERT_HIGH_TEMPERATURE,
                       &data,
                       app_config.temp_threshold);
        }

        if (data.humidity > app_config.humidity_threshold) {
            send_alert(ALERT_HIGH_HUMIDITY,
                       &data,
                       app_config.humidity_threshold);
        }

        if (has_previous) {
            float difference = fabsf(
                data.temperature - previous_temperature
            );

            if (difference > TEMP_CHANGE_THRESHOLD) {
                send_alert(ALERT_TEMP_CHANGE,
                           &data,
                           TEMP_CHANGE_THRESHOLD);
            }
        }

        previous_temperature = data.temperature;
        has_previous = 1;

        if (app_config.verbose) {
            printf("[SENSOR] Time=%llu Temp=%.2f C "
                   "Humidity=%.2f %% | Avg Temp=%.2f C "
                   "Avg Humidity=%.2f %%\n",
                   (unsigned long long)data.timestamp,
                   data.temperature,
                   data.humidity,
                   avg_temp,
                   avg_humidity);
            fflush(stdout);
        }

        send_processed_log(&data, avg_temp, avg_humidity);
    }

    return NULL;
}

