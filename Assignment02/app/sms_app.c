/*
 * File: sms_app.c
 * Purpose: Main program for Sensor Monitoring System.
 * Author: Student
 * Student ID: SE204292
 * Date: 12/11/2026
 */
#include <math.h>
#include <limits.h>
#include "sms_app.h"
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

/* Bien toan cuc */
CircularBuffer sensor_buffer;

AppConfig app_config = {
    .interval_ms = DEFAULT_INTERVAL_MS,
    .temp_threshold = DEFAULT_TEMP_THRESHOLD,
    .humidity_threshold = DEFAULT_HUMIDITY_THRESHOLD,
    .duration_sec = DEFAULT_DURATION_SEC,
    .verbose = 0,
    .sched_policy = SCHED_OTHER
};

mqd_t log_queue = (mqd_t)-1;
mqd_t alarm_queue = (mqd_t)-1;

volatile sig_atomic_t app_running = 1;

/* Xu ly signal */
static void signal_handler(int signal_number)
{
    (void)signal_number;
    app_running = 0;
}

/* Hien thi huong dan */
static void print_usage(const char *program)
{
    printf("  -s <mode>  Scheduling policy: other or fifo\n");
    printf("Usage: %s [OPTIONS]\n", program);
    printf("  -i <ms>    Sampling interval (default: 500)\n");
    printf("  -t <temp>  Temperature threshold (default: 40.0)\n");
    printf("  -h <hum>   Humidity threshold (default: 80.0)\n");
    printf("  -d <sec>   Run duration, 0 = infinite (default: 60)\n");
    printf("  -v         Verbose sensor output\n");
    printf("  --help     Display this help\n");
}

/* Doc tham so dong lenh */
static int parse_arguments(int argc, char *argv[])
{
    int option;
    char *endptr;

    static const struct option long_options[] = {
        {"help", no_argument, NULL, 1000},
        {NULL, 0, NULL, 0}
    };

    while ((option = getopt_long(argc, argv,
                                 "i:t:h:d:vs:",
                                 long_options, NULL)) != -1) {
        switch (option) {

        case 'i': {
            errno = 0;
            unsigned long value = strtoul(optarg, &endptr, 10);

            if (errno != 0 || endptr == optarg ||
                *endptr != '\0' || value < 1 || value > 60000) {
                fprintf(stderr, "Invalid interval: %s\n", optarg);
                return -1;
            }

            app_config.interval_ms = (unsigned int)value;
            break;
        }

        case 't': {
            errno = 0;
            float value = strtof(optarg, &endptr);

            if (errno != 0 || endptr == optarg ||
                *endptr != '\0' || !isfinite(value)) {
                fprintf(stderr, "Invalid temperature threshold: %s\n",
                        optarg);
                return -1;
            }

            app_config.temp_threshold = value;
            break;
        }

        case 'h': {
            errno = 0;
            float value = strtof(optarg, &endptr);

            if (errno != 0 || endptr == optarg ||
                *endptr != '\0' || !isfinite(value)) {
                fprintf(stderr, "Invalid humidity threshold: %s\n",
                        optarg);
                return -1;
            }

            app_config.humidity_threshold = value;
            break;
        }

        case 'd': {
            errno = 0;
            unsigned long value = strtoul(optarg, &endptr, 10);

            if (errno != 0 || endptr == optarg ||
                *endptr != '\0' || value > UINT_MAX) {
                fprintf(stderr, "Invalid duration: %s\n", optarg);
                return -1;
            }

            app_config.duration_sec = (unsigned int)value;
            break;
        }

        case 'v':
            app_config.verbose = 1;
            break;

        case 's':
            if (strcmp(optarg, "fifo") == 0) {
                app_config.sched_policy = SCHED_FIFO;
            } else if (strcmp(optarg, "other") == 0) {
                app_config.sched_policy = SCHED_OTHER;
            } else {
                fprintf(stderr,
                        "Invalid scheduling mode. Use fifo or other.\n");
                return -1;
            }
            break;

        case 1000:
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);

        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
        return -1;
    }

    return 0;
}
/* Gui log message den Logger Thread */
/* Gui log message den Logger Thread */
int send_log_message(const char *message)
{
    LogMessage log_msg = {0};

    if (message == NULL || log_queue == (mqd_t)-1)
        return -1;

    snprintf(log_msg.message, sizeof(log_msg.message), "%s", message);

    int ret;

    do {
        ret = mq_send(log_queue,
                      (const char *)&log_msg,
                      sizeof(log_msg), 0);
    } while (ret == -1 && errno == EINTR);

    if (ret == -1) {
        if (errno == EAGAIN) {
            fprintf(stderr, "[WARNING] Log queue full, message dropped.\n");
        } else {
            perror("Cannot send log message");
        }
    }

    return ret;
}
/* Tao POSIX message queues */
static int create_message_queues(void)
{
    struct mq_attr attr;

    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(LogMessage);

    mq_unlink(LOG_QUEUE_NAME);

    log_queue = mq_open(LOG_QUEUE_NAME,
                        O_CREAT | O_RDWR | O_NONBLOCK,
                        0666, &attr);

    if (log_queue == (mqd_t)-1) {
        perror("Cannot create log queue");
        return -1;
    }

    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(AlertMessage);

    mq_unlink(ALARM_QUEUE_NAME);

    alarm_queue = mq_open(ALARM_QUEUE_NAME,
                          O_CREAT | O_RDWR | O_NONBLOCK,
                          0666, &attr);

    if (alarm_queue == (mqd_t)-1) {
        perror("Cannot create alarm queue");

        mq_close(log_queue);
        mq_unlink(LOG_QUEUE_NAME);
        log_queue = (mqd_t)-1;

        return -1;
    }

    return 0;
}

/* Dong va xoa message queues */
static void close_message_queues(void)
{
    if (log_queue != (mqd_t)-1) {
        mq_close(log_queue);
        mq_unlink(LOG_QUEUE_NAME);
        log_queue = (mqd_t)-1;
    }

    if (alarm_queue != (mqd_t)-1) {
        mq_close(alarm_queue);
        mq_unlink(ALARM_QUEUE_NAME);
        alarm_queue = (mqd_t)-1;
    }
}

/* Chuong trinh chinh */
int main(int argc, char *argv[])
{
    pthread_t sensor_tid;
    pthread_t processor_tid;
    pthread_t alert_tid;
    pthread_t logger_tid;

    int sensor_created = 0;
    int processor_created = 0;
    int alert_created = 0;
    int logger_created = 0;

    int ret;

    /* 1. Doc tham so dong lenh */
    if (parse_arguments(argc, argv) != 0)
        return EXIT_FAILURE;

    /* 2. Dang ky signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

#ifdef __arm__
    /* Khoa bo nho cho real-time application */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
        perror("Warning: mlockall failed");
#endif

    printf("\n====================================\n");
    printf("   SENSOR MONITORING SYSTEM\n");
    printf("====================================\n");
    printf("Sampling interval : %u ms\n", app_config.interval_ms);
    printf("Temperature limit: %.2f C\n",
           app_config.temp_threshold);
    printf("Humidity limit   : %.2f %%\n",
           app_config.humidity_threshold);
    printf("Duration         : %u seconds\n",
           app_config.duration_sec);
    printf("Scheduling policy: %s\n",
           app_config.sched_policy == SCHED_FIFO
               ? "SCHED_FIFO" : "SCHED_OTHER");
    printf("====================================\n\n");

    /* 3. Khoi tao circular buffer */
    if (buffer_init(&sensor_buffer) != 0) {
        fprintf(stderr, "Cannot initialize circular buffer\n");
        return EXIT_FAILURE;
    }

    /* 4. Tao message queues */
    if (create_message_queues() != 0) {
        buffer_destroy(&sensor_buffer);
        return EXIT_FAILURE;
    }

    /* 5. Tao cac thread */

    ret = pthread_create(&logger_tid, NULL, logger_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "Cannot create Logger Thread: %s\n",
                strerror(ret));
        goto shutdown;
    }
    logger_created = 1;

    ret = pthread_create(&alert_tid, NULL, alert_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "Cannot create Alert Thread: %s\n",
                strerror(ret));
        goto shutdown;
    }
    alert_created = 1;

    ret = pthread_create(&processor_tid, NULL,
                         processor_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "Cannot create Processor Thread: %s\n",
                strerror(ret));
        goto shutdown;
    }
    processor_created = 1;

    ret = pthread_create(&sensor_tid, NULL, sensor_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "Cannot create Sensor Thread: %s\n",
                strerror(ret));
        goto shutdown;
    }
    sensor_created = 1;

    printf("All threads started successfully.\n");

    /* 6. Cho ung dung chay */
    if (app_config.duration_sec == 0) {
        while (app_running)
            sleep(1);
    } else {
        unsigned int elapsed = 0;

        while (app_running &&
               elapsed < app_config.duration_sec) {
            sleep(1);
            elapsed++;
        }
    }

shutdown:

    printf("\nStopping application...\n");

    app_running = 0;

    /* 7. Doi Sensor Thread ket thuc */
    if (sensor_created)
        pthread_join(sensor_tid, NULL);

    /* 8. Gui sentinel cho Processor Thread */
    if (processor_created) {
        SensorData stop_data = {0};

        while (!buffer_push(&sensor_buffer, stop_data))
            usleep(10000);

        pthread_join(processor_tid, NULL);
    }

    /* 9. Gui sentinel cho Alert Thread */
    if (alert_created) {
        AlertMessage stop_alert = {0};

        while (mq_send(alarm_queue,
                       (const char *)&stop_alert,
                       sizeof(stop_alert), 0) == -1) {

            if (errno == EINTR)
                continue;

            if (errno == EAGAIN) {
                usleep(10000);
                continue;
            }

            perror("mq_send alert sentinel");
            break;
        }

        pthread_join(alert_tid, NULL);
    }

    /* 10. Gui sentinel cho Logger Thread */
    if (logger_created) {
        LogMessage stop_log = {0};

        while (mq_send(log_queue,
                       (const char *)&stop_log,
                       sizeof(stop_log), 0) == -1) {

            if (errno == EINTR)
                continue;

            if (errno == EAGAIN) {
                usleep(10000);
                continue;
            }

            perror("mq_send logger sentinel");
            break;
        }

        pthread_join(logger_tid, NULL);
    }

    /* 11. Giai phong tai nguyen */
    close_message_queues();

    buffer_destroy(&sensor_buffer);

    printf("Application stopped.\n");

    return EXIT_SUCCESS;
}
