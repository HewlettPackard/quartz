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

#include "graph.h"

#include <assert.h>
#include <stdlib.h>

/**
 * Basic directed graph type.
 *
 * The implementation uses adjacency lists
 * represented as variable-length arrays.
 *
 * These arrays may or may not be sorted: if one gets long enough
 * and you call graph_has_edge on its source, it will be 
 */
struct graph {
    int n;              /* number of vertices */
    int m;              /* number of edges */
    struct successors {
        int ind;        /* indegree */
        int d;          /* number of successors (outdegree)*/
        int len;        /* number of slots in array */
        char is_sorted; /* true if list is already sorted */
        int list[1];    /* actual list of successors */
    } *alist[1];
};

graph_t* graph_create(int n)
{
    graph_t* g;
    int i;

    g = malloc(sizeof(struct graph) + sizeof(struct successors *) * (n-1));
    assert(g);

    g->n = n;
    g->m = 0;

    for(i = 0; i < n; i++) {
        g->alist[i] = malloc(sizeof(struct successors));
        assert(g->alist[i]);

        g->alist[i]->d = 0;
        g->alist[i]->ind = 0;
        g->alist[i]->len = 1;
        g->alist[i]->is_sorted= 1;
    }
    
    return g;
}

/* when we are willing to call bsearch */
#define BSEARCH_THRESHOLD (10)

static int intcmp(const void *a, const void *b)
{
    return *((const int *) a) - *((const int *) b);
}

int graph_find_edge(graph_t* g, int source, int sink, int* slot)
{
    int i;

    assert(source >= 0);
    assert(source < g->n);
    assert(sink >= 0);
    assert(sink < g->n);

    if(graph_out_degree(g, source) >= BSEARCH_THRESHOLD) {
        /* make sure it is sorted */
        if(! g->alist[source]->is_sorted) {
            qsort(g->alist[source]->list,
                    g->alist[source]->d,
                    sizeof(int),
                    intcmp);
        }
        
        /* call bsearch to do binary search for us */
        int* lp = bsearch(&sink,
                    g->alist[source]->list,
                    g->alist[source]->d,
                    sizeof(int),
                    intcmp);
        if (lp && slot) {
            *slot = lp - g->alist[source]->list; 
        }
        return lp != 0;
    } else {
        /* just do a simple linear search */
        /* we could call lfind for this, but why bother? */
        for(i = 0; i < g->alist[source]->d; i++) {
            if(g->alist[source]->list[i] == sink) {
                if (slot) *slot = i;
                return 1;
            }
        }
        /* else */
        return 0;
    }
}

void graph_destroy(graph_t* g)
{
    int i;

    for(i = 0; i < g->n; i++) free(g->alist[i]);
    free(g);
}

void graph_add_edge(graph_t* g, int u, int v)
{
    assert(u >= 0);
    assert(u < g->n);
    assert(v >= 0);
    assert(v < g->n);

    /* do we need to grow the list? */
    while(g->alist[u]->d >= g->alist[u]->len) {
        g->alist[u]->len *= 2;
        g->alist[u] =
            realloc(g->alist[u], 
                sizeof(struct successors) + sizeof(int) * (g->alist[u]->len - 1));
    }

    /* now add the new sink */
    g->alist[u]->list[g->alist[u]->d++] = v;
    g->alist[u]->is_sorted = 0;

    /* bump the sink's indegree */
    g->alist[v]->ind++;

    /* bump edge count */
    g->m++;
}

/* remove an existing edge from a graph */
void
graph_remove_edge(graph_t* g, int u, int v)
{
    assert(u >= 0);
    assert(u < g->n);
    assert(v >= 0);
    assert(v < g->n);

    int sink_slot;
    if (graph_find_edge(g, u, v, &sink_slot)) {
        if (g->alist[u]->d > 1) {
            g->alist[u]->list[sink_slot] = g->alist[u]->list[g->alist[u]->d-1];
        }
        g->alist[u]->d--;
        g->alist[u]->is_sorted = 0;

        /* decrement the sink's indegree */
        g->alist[v]->ind--;

        /* decrement edge count */
        g->m--;
    }
}

int graph_vertex_count(graph_t* g)
{
    return g->n;
}

int graph_edge_count(graph_t* g)
{
    return g->m;
}

int graph_out_degree(graph_t* g, int source)
{
    assert(source >= 0);
    assert(source < g->n);

    return g->alist[source]->d;
}

int graph_in_degree(graph_t* g, int source)
{
    assert(source >= 0);
    assert(source < g->n);

    return g->alist[source]->ind;
}

int graph_has_edge(graph_t* g, int source, int sink)
{
    return graph_find_edge(g, source, sink, NULL);
}

void graph_foreach(graph_t* g, int source,
    void (*f)(graph_t* g, int source, int sink, void *data),
    void *data)
{
    int i;

    assert(source >= 0);
    assert(source < g->n);

    for(i = 0; i < g->alist[source]->d; i++) {
        f(g, source, g->alist[source]->list[i], data);
    }
}

struct queue {
    int size;
    int rear;
    int front;
    int* elem;
};


static struct queue* queue_create(size_t size)
{
    struct queue* q = malloc(sizeof(*q));

    q->elem = calloc(size, sizeof(*q->elem));
    q->rear = -1;
    q->front = -1;
    q->size = size;

    return q;
}

static int queue_is_full(struct queue* q)
{
    return (q->front == 0 && q->rear == q->size-1) || (q->front == q->rear+1);
}

static int queue_is_empty(struct queue* q)
{
    return q->front == -1;
}

static int queue_enqueue(struct queue* q, int elem)
{
    if (queue_is_full(q)) return -1;

    if (q->front == -1) q->front = 0;
    q->rear = (q->rear+1) % q->size;
    q->elem[q->rear] = elem;
    return 0;
}

static int queue_dequeue(struct queue* q)
{
    if (queue_is_empty(q)) return -1;

    int elem = q->elem[q->front];
    if (q->front == q->rear) {
        q->front = q->rear = -1;
    } else {
        q->front = (q->front+1) % q->size;
    }
    return elem;
}

int* graph_topo_sort(graph_t* g)
{
    int u;
    int* topo_sort = calloc(g->n, sizeof(int));

    struct queue* q = queue_create(g->n);    

    for(u = 0; u < g->n; u++) {
        if (graph_in_degree(g, u) == 0) queue_enqueue(q, u);
    }
    int count = 0;
    while (!queue_is_empty(q) && count < g->n) {
        u = queue_dequeue(q);
        topo_sort[count++] = u;
        while (g->alist[u]->d) {
            int v = g->alist[u]->list[0];
            graph_remove_edge(g, u, v);
            if ((graph_in_degree(g, v) == 0)) queue_enqueue(q, v);
        }
    }
    
    return topo_sort;
}
