#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

#define BUFFER_SIZE 64

typedef struct {
    uint64_t timestamp;
    float temperature;
    float humidity;
} SensorData;

typedef struct {
    SensorData *data;

    unsigned int head;
    unsigned int tail;
    unsigned int current_size;

    unsigned long total_produced;
    unsigned long total_consumed;
    unsigned long overflow_count;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} CircularBuffer;

int buffer_init(CircularBuffer *buffer);
void buffer_destroy(CircularBuffer *buffer);

bool buffer_push(CircularBuffer *buffer, SensorData item);
bool buffer_pop(CircularBuffer *buffer, SensorData *item);

void buffer_get_stats(CircularBuffer *buffer,
                      unsigned int *current_size,
                      unsigned long *produced,
                      unsigned long *consumed,
                      unsigned long *overflow);

#endif
