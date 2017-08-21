#include <assert.h>

#include "graph.h"


int test_graph()
{
    graph_t* g = graph_create(10);

    graph_add_edge(g, 0, 3);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 1, 1);
    graph_add_edge(g, 2, 1);

    assert(graph_has_edge(g, 0, 1));
    assert(graph_has_edge(g, 0, 3));
    assert(graph_has_edge(g, 1, 1));
    assert(graph_has_edge(g, 2, 1));
    assert(graph_has_edge(g, 3, 1) != 1);

    graph_remove_edge(g, 0, 3);
    graph_remove_edge(g, 0, 1);
    assert(graph_has_edge(g, 0, 1) != 1);
    assert(graph_has_edge(g, 0, 3) != 1);

    graph_remove_edge(g, 1, 1);
    assert(graph_has_edge(g, 1, 1) != 1);
}

int main()
{
    test_graph();
}
