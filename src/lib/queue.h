#ifndef __QUEUE_H
#define __QUEUE_H

#define MAX_SIZE 128

typedef struct {
    void* A[MAX_SIZE];
    int front;
    int rear;
    int size;
} queue_t;

void queue_init(queue_t* q, int size);
int queue_is_empty(queue_t* q);
int queue_is_full(queue_t* q);
int queue_enqueue(queue_t* q, void* x);
int queue_dequeue(queue_t* q);
int queue_front(queue_t* q, void** x);

#endif // __QUEUE_H
