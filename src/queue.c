/* queue.c: Concurrent Queue of Requests */

#include "mq/queue.h"

/**
 * Create queue structure.
 * @return  Newly allocated queue structure.
 */
Queue * queue_create() {
    // Allocates queue structure
    Queue *q = calloc(1, sizeof(Queue));

    // Checks if q was created
    if (!q) {
        return NULL;
    }

    // Sets queue size
    q->size = 0;
    
    mutex_init(&q->lock, NULL);         // Initializes lock
    cond_init(&q->consumer, NULL);      // Initializes condition variable
    cond_init(&q->producer, NULL);      // Initializes condition variable

    return q;
}

/**
 * Delete queue structure.
 * @param   q       Queue structure.
 */
void queue_delete(Queue *q) {
    // If q exists, frees the queue
    if (q) {
        // Frees the requests in the queue
        while (q->head) {
            Request *temp = q->head->next;
            request_delete(q->head);
            q->head = temp;
        }

        free(q);
    }

    return;
}

/**
 * Push request to the back of queue.
 * @param   q       Queue structure.
 * @param   r       Request structure.
 */
void queue_push(Queue *q, Request *r) {
    // Locks critical section
    mutex_lock(&q->lock); 

    r->next = NULL; // Request is not yet pointing to any other requests

    // If the queue is empty, sets the head to the request
    if (q->size == 0) {
        q->head = r;
    }

    // Current tail now points to request
    else {
        q->tail->next = r;
    }

    // Sets the tail to the request and increment size
    q->tail = r;
    q->size++;

    cond_signal(&q->producer);              // Wakes up consumer thread
    mutex_unlock(&q->lock);                 // Unlocks critical section

    return;
}

/**
 * Pop request to the front of queue (block until there is something to return).
 * @param   q       Queue structure.
 * @return  Request structure.
 */
Request * queue_pop(Queue *q) {
    // Locks critical section
    mutex_lock(&q->lock);

    while (q->size == 0) {
        cond_wait(&q->producer, &q->lock);  // Yields and waits until woken up
    }

    Request *temp = q->head;

    // If the queue will be empty, sets head and tail to NULL
    if (q->size == 1) {
        q->head = NULL;
        q->tail = NULL;
    }

    // If there will be one request in queue, sets head to tail
    else if (q->size == 2) {
        q->head = q->tail;
    }

    // Sets head to its next request
    else {
        q->head = q->head->next;
    }
    
    q->size--;

    // Unlocks critical section
    mutex_unlock(&q->lock); 

    return temp;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
