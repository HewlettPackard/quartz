#include "queue.h"

void queue_init(queue_t* q, int size)
{
    q->front = -1;
    q->rear = -1;
    q->size = size;
}

int queue_is_empty(queue_t* q)
{
    return (q->front == -1 && q->rear == -1);
}

int queue_is_full(queue_t* q)
{
    return (q->rear+1) % q->size == q->front ? 1: 0;
}

int queue_enqueue(queue_t* q, void* x)
{
    if (queue_is_full(q)) {
        return -1;
    }
    
    if (queue_is_empty(q)) {
        q->front = q->rear = 0;
    } else {
        q->rear = (q->rear+1) % q->size;
    }
    q->A[q->rear] = x;
    
    return 0;
}

int queue_dequeue(queue_t* q)
{
    if (queue_is_empty(q)) {
        return -1;
    } else if (q->front == q->rear) {
        q->front = q->rear = -1;
    } else {
        q->front = (q->front + 1) % q->size;
    }
    return 0;
}

int queue_front(queue_t* q, void** x) 
{
    if (q->front == -1) {
        return -1;
    }
    *x = q->A[q->front];
    return 0;
}
