/*
 * File: sms_app.h
 * Purpose: Shared definitions for Sensor Monitoring System.
 * Author: Student
 * Student ID: SE204292
 * Date: 12/11/2026
 */

#ifndef SMS_APP_H
#define SMS_APP_H
#include <sched.h>
#include <pthread.h>
#include <mqueue.h>
#include <signal.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include "circular_buffer.h"
/* Kernel driver IOCTL */
#define SMS_MAGIC 'S'
#define SMS_SET_INTERVAL _IOW(SMS_MAGIC, 1, unsigned int)
/* Duong dan device va log */
#define SENSOR_DEVICE "/dev/sms_sensor"
#define LOG_FILE "/tmp/sms.log"

/* Ten POSIX message queue */
#define LOG_QUEUE_NAME "/sms_log_queue"
#define ALARM_QUEUE_NAME "/sms_alarm_queue"

/* Cau hinh mac dinh */
#define DEFAULT_INTERVAL_MS 500
#define DEFAULT_TEMP_THRESHOLD 40.0f
#define DEFAULT_HUMIDITY_THRESHOLD 80.0f
#define DEFAULT_DURATION_SEC 60

/* Kich thuoc message */
#define LOG_MSG_SIZE 256

/* Loai canh bao */
typedef enum {
    ALERT_HIGH_TEMPERATURE = 1,
    ALERT_HIGH_HUMIDITY,
    ALERT_TEMP_CHANGE
} AlertType;

/* Message gui den Logger Thread */
typedef struct {
    char message[LOG_MSG_SIZE];
} LogMessage;

/* Message gui den Alert Thread */
typedef struct {
    uint64_t timestamp;
    AlertType type;
    float value;
    float threshold;
} AlertMessage;

/* Cau hinh ung dung */
typedef struct {
    unsigned int interval_ms;
    float temp_threshold;
    float humidity_threshold;
    unsigned int duration_sec;
    int verbose;
    int sched_policy;
} AppConfig;

/* Bien dung chung */
extern CircularBuffer sensor_buffer;
extern AppConfig app_config;

extern mqd_t log_queue;
extern mqd_t alarm_queue;

extern volatile sig_atomic_t app_running;

/* Khai bao cac thread */
void *sensor_thread(void *arg);
void *processor_thread(void *arg);
void *alert_thread(void *arg);
void *logger_thread(void *arg);

/* Gui log message */
int send_log_message(const char *message);

#endif
