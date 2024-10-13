#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "job_queue.h"


int job_queue_init(struct job_queue *job_queue, int capacity) {
    job_queue->queue = malloc(capacity * sizeof(void *));
    if (!job_queue->queue) return -1;
    job_queue->head = 0;
    job_queue->tail = 0;
    job_queue->size = 0;
    job_queue->capacity = capacity;
    job_queue->destroy = 0;
    pthread_mutex_init(&job_queue->mutex, NULL);
    pthread_cond_init(&job_queue->not_empty, NULL);
    pthread_cond_init(&job_queue->not_full, NULL);
    return 0;
}

int job_queue_destroy(struct job_queue *job_queue) {
    pthread_mutex_lock(&job_queue->mutex);
    job_queue->destroy = 1;
    pthread_cond_broadcast(&job_queue->not_empty);
    pthread_cond_broadcast(&job_queue->not_full);
    while (job_queue->size > 0) {
        pthread_cond_wait(&job_queue->not_empty, &job_queue->mutex);
    }
    pthread_mutex_unlock(&job_queue->mutex);
    free(job_queue->queue);
    pthread_mutex_destroy(&job_queue->mutex);
    pthread_cond_destroy(&job_queue->not_empty);
    pthread_cond_destroy(&job_queue->not_full);
    return 0;
}

int job_queue_push(struct job_queue *job_queue, void *data) {
    pthread_mutex_lock(&job_queue->mutex);
    while (job_queue->size == job_queue->capacity && !job_queue->destroy) {
        pthread_cond_wait(&job_queue->not_full, &job_queue->mutex);
    }
    if (job_queue->destroy) {
        pthread_mutex_unlock(&job_queue->mutex);
        return -1;
    }
    job_queue->queue[job_queue->tail] = data;
    job_queue->tail = (job_queue->tail + 1) % job_queue->capacity;
    job_queue->size++;
    pthread_cond_signal(&job_queue->not_empty);
    pthread_mutex_unlock(&job_queue->mutex);
    return 0;
}

int job_queue_pop(struct job_queue *job_queue, void **data) {
    pthread_mutex_lock(&job_queue->mutex);
    while (job_queue->size == 0 && !job_queue->destroy) {
        pthread_cond_wait(&job_queue->not_empty, &job_queue->mutex);
    }
    if (job_queue->destroy && job_queue->size == 0) {
        pthread_mutex_unlock(&job_queue->mutex);
        return -1;
    }
    *data = job_queue->queue[job_queue->head];
    job_queue->head = (job_queue->head + 1) % job_queue->capacity;
    job_queue->size--; 
    if (job_queue->size == 0 && job_queue->destroy) {
        pthread_cond_signal(&job_queue->not_empty);
    }
    pthread_cond_signal(&job_queue->not_full);
    pthread_mutex_unlock(&job_queue->mutex);
    return 0;
}
