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
