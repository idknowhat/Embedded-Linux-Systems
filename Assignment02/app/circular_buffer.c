/*
 * File: circular_buffer.c
 * Purpose: Implement circular buffer for sensor data.
 * Author: Student
 * Student ID: SE204292
 * Date: 12/11/2026
 */

#include "circular_buffer.h"

#include <stdlib.h>

/* Khoi tao circular buffer */
int buffer_init(CircularBuffer *buffer)
{
    if (buffer == NULL)
        return -1;

    buffer->data = calloc(BUFFER_SIZE, sizeof(SensorData));

    if (buffer->data == NULL)
        return -1;

    buffer->head = 0;
    buffer->tail = 0;
    buffer->current_size = 0;

    buffer->total_produced = 0;
    buffer->total_consumed = 0;
    buffer->overflow_count = 0;

    if (pthread_mutex_init(&buffer->mutex, NULL) != 0) {
        free(buffer->data);
        buffer->data = NULL;
        return -1;
    }

    if (pthread_cond_init(&buffer->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&buffer->mutex);
        free(buffer->data);
        buffer->data = NULL;
        return -1;
    }

    if (pthread_cond_init(&buffer->not_full, NULL) != 0) {
        pthread_cond_destroy(&buffer->not_empty);
        pthread_mutex_destroy(&buffer->mutex);
        free(buffer->data);
        buffer->data = NULL;
        return -1;
    }

    return 0;
}

/* Giai phong circular buffer */
void buffer_destroy(CircularBuffer *buffer)
{
    if (buffer == NULL)
        return;

    pthread_mutex_lock(&buffer->mutex);

    free(buffer->data);
    buffer->data = NULL;

    pthread_mutex_unlock(&buffer->mutex);

    pthread_cond_destroy(&buffer->not_empty);
    pthread_cond_destroy(&buffer->not_full);
    pthread_mutex_destroy(&buffer->mutex);
}

/* Them mot mau cam bien vao buffer */
bool buffer_push(CircularBuffer *buffer, SensorData item)
{
    if (buffer == NULL)
        return false;

    pthread_mutex_lock(&buffer->mutex);

    /* Buffer day: bo qua mau moi */
    if (buffer->current_size >= BUFFER_SIZE) {
        buffer->overflow_count++;

        pthread_mutex_unlock(&buffer->mutex);
        return false;
    }

    buffer->data[buffer->head] = item;

    buffer->head = (buffer->head + 1) % BUFFER_SIZE;
    buffer->current_size++;
    buffer->total_produced++;

    /* Thong bao cho thread dang cho du lieu */
    pthread_cond_signal(&buffer->not_empty);

    pthread_mutex_unlock(&buffer->mutex);

    return true;
}

/* Lay mot mau cam bien khoi buffer */
bool buffer_pop(CircularBuffer *buffer, SensorData *item)
{
    if (buffer == NULL || item == NULL)
        return false;

    pthread_mutex_lock(&buffer->mutex);

    /* Cho den khi co du lieu */
    while (buffer->current_size == 0) {
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
    }

    *item = buffer->data[buffer->tail];

    buffer->tail = (buffer->tail + 1) % BUFFER_SIZE;
    buffer->current_size--;
    buffer->total_consumed++;

    /* Thong bao buffer vua co cho trong */
    pthread_cond_signal(&buffer->not_full);

    pthread_mutex_unlock(&buffer->mutex);

    return true;
}

/* Lay thong ke cua buffer */
void buffer_get_stats(CircularBuffer *buffer,
                      unsigned int *current_size,
                      unsigned long *produced,
                      unsigned long *consumed,
                      unsigned long *overflow)
{
    if (buffer == NULL)
        return;

    pthread_mutex_lock(&buffer->mutex);

    if (current_size != NULL)
        *current_size = buffer->current_size;

    if (produced != NULL)
        *produced = buffer->total_produced;

    if (consumed != NULL)
        *consumed = buffer->total_consumed;

    if (overflow != NULL)
        *overflow = buffer->overflow_count;

    pthread_mutex_unlock(&buffer->mutex);
}

