/*
 * Copyright 2016 Hewlett Packard Enterprise Development LP.
 *
 * This program is free software; you can redistribute it and.or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version. This program is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details. You
 * should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
