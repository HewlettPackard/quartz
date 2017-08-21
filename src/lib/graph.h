/***************************************************************************
Copyright 2016 Hewlett Packard Enterprise Development LP.  
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version. This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details. You
should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
***************************************************************************/
#ifndef __GRAPH_H
#define __GRAPH_H

/* basic directed graph type */
typedef struct graph graph_t;

/* create a new graph with n vertices labeled 0..n-1 and no edges */
graph_t* graph_create(int n);

/* free all space used by graph */
void graph_destroy(graph_t*);

/* add an edge to an existing graph */
/* doing this more than once may have unpredictable results */
void graph_add_edge(graph_t*, int source, int sink);

/* remove an existing edge from a graph */
void graph_remove_edge(graph_t*, int source, int sink);

/* return the number of vertices in the graph */
int graph_vertex_count(graph_t*);

/* return the number of edges in the graph */
int graph_edge_count(graph_t*);

/* return the out-degree of a vertex */
int graph_out_degree(graph_t*, int source);

/* return the in-degree of a vertex */
int graph_in_degree(graph_t*, int source);

/* return 1 if edge (source, sink) exists), 0 otherwise */
int graph_has_edge(graph_t*, int source, int sink);

/* invoke f on all edges (u,v) with source u */
/* supplying data as final parameter to f */
/* no particular order is guaranteed */
void graph_foreach(graph_t* g, int source,
        void (*f)(graph_t* g, int source, int sink, void *data),
        void *data);

/** \brief Topological sort */
int* graph_topo_sort(graph_t* g);

#endif // __GRAPH_H
