#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <attr/xattr.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#include "error.h"
#include "cpu/cpu.h"
#include "topology.h"

struct arg_cmd
{
    struct arg_global* global;
    char* name;
    int size;
};

static struct argp_option opt_cmd[] =
{
    { "size", 'z', "sz", 0,
        "Atomic size in bytes." },

    { 0 }
};

static char doc_cmd[] =
  "\n"
  "The aa doc prefix."
  "\v"
  "The aa doc suffix."
  ;

static error_t
parse_cmd(int key, char* arg, struct argp_state* state)
{
    struct arg_cmd* argd = state->input;
  
    assert(argd);
    assert(argd->global);

    switch(key)
    {
        case 'z':
            argd->size = atoi(arg);
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp_cmd =
{
    opt_cmd,
    parse_cmd,
    0,
    doc_cmd
};

int save_mc_pci_topology(const char* path, physical_node_t* physical_nodes[], int num_physical_nodes);
int discover_mc_pci_topology(cpu_model_t* cpu_model, physical_node_t* physical_nodes[], int num_physical_nodes);

void cmd_make(struct argp_state* state)
{
    struct arg_cmd argd = { 0, 0, 0};
    int    argc = state->argc - state->next + 1;
    char** argv = &state->argv[state->next - 1];
    char*  argv0 =  argv[0];

    argd.global = state->input;

    argv[0] = malloc(strlen(state->name) + strlen(" make") + 1);

    if(!argv[0])
        argp_failure(state, 1, ENOMEM, 0);

    sprintf(argv[0], "%s make", state->name);

    argp_parse(&argp_cmd, argc, argv, ARGP_IN_ORDER, &argc, &argd);

    free(argv[0]);

    argv[0] = argv0;

    state->next += argc - 1;

    cpu_model_t* cpu;
    if ((cpu = cpu_model()) == NULL) {
        printf("wrong model\n");
    }

#if 1
    physical_topology_t* pt;
    discover_physical_topology(cpu, &pt);
    physical_topology_to_xml(pt, "/tmp/test");

#else
    physical_topology_t* pt;
    physical_topology_from_xml(cpu, "/tmp/test", &pt);
    physical_topology_to_xml(pt, "/tmp/test2");
#endif
    return 0;

    if ((cpu = cpu_model()) == NULL) {
        printf("wrong model\n");
    }

    char* physical_nodes_str = "1,2";

    physical_node_t** physical_nodes = NULL;
    int num_physical_nodes;
    int node_id;
    physical_node_t* node_i, *node_j, *sibling_node;

    // parse the physical nodes string
    int hyperthreading = 0;
    int n, v, i, j, sibling_idx;
    struct bitmask* mem_nodes;
    int* physical_node_ids;
    char* saveptr;
    char* token = "NULL";
    int ret;
    physical_node_ids = calloc(numa_num_possible_nodes(), sizeof(*physical_node_ids));
    num_physical_nodes = 2;

    physical_node_ids[0] = 0;
    physical_node_ids[1] = 1;

    if (!(physical_nodes = calloc(num_physical_nodes, sizeof(*physical_nodes)))) {
        DBG_LOG(ERROR, "Failed physical nodes allocation\n");
        abort();
    }

    // select those nodes we can run on (e.g. not constrained by any numactl)
    mem_nodes = numa_get_mems_allowed();
    for (i=0, n=0; i<num_physical_nodes; i++) {
        node_id = physical_node_ids[i];
        if (numa_bitmask_isbitset(mem_nodes, node_id)) {
            physical_nodes[n] = malloc(sizeof(**physical_nodes));
            memset(physical_nodes[n], 0, sizeof(**physical_nodes));
            physical_nodes[n]->node_id = node_id;
            physical_nodes[n]->cpu_bitmask = numa_allocate_cpumask();
            physical_nodes[n]->cpu_model = cpu_model;
            numa_node_to_cpus(node_id, physical_nodes[n]->cpu_bitmask);
            if (hyperthreading) {
                physical_nodes[n]->num_cpus = num_cpus(physical_nodes[n]->cpu_bitmask);
            } else {
                DBG_LOG(INFO, "Not using hyperthreading.\n");
                // disable the upper half of the processors in the bitmask
                physical_nodes[n]->num_cpus = num_cpus(physical_nodes[n]->cpu_bitmask) / 2;
                int fc = first_cpu(physical_nodes[n]->cpu_bitmask);
                for (j=fc+system_num_cpus()/2; j<fc+system_num_cpus()/2+physical_nodes[n]->num_cpus; j++) {
                    if (numa_bitmask_isbitset(physical_nodes[n]->cpu_bitmask, j)) {
                        numa_bitmask_clearbit(physical_nodes[n]->cpu_bitmask, j);
                    }
                }
            }
            DBG_LOG(INFO, "%d CPUs on physical node %d\n", physical_nodes[n]->num_cpus, n);
            n++;
        }
    }
    free(physical_node_ids);
    num_physical_nodes = n;



    char* mc_pci_file = "/tmp/pci";
    discover_mc_pci_topology(cpu, physical_nodes, num_physical_nodes);
    save_mc_pci_topology(mc_pci_file, physical_nodes, num_physical_nodes);
 
done:
    free(physical_nodes);
    return;
}
