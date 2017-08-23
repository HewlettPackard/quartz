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

#include <libconfig.h>
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
    const char* physical_topo_filename;
};

static struct argp_option opt_cmd[] =
{
    { 0 }
};

static char doc_cmd[] =
  "\n"
  "Discover and save virtual topology in FILE."
  "\v"
  ;

static error_t
parse_cmd(int key, char* arg, struct argp_state* state)
{
    struct arg_cmd* argd = state->input;
  
    assert(argd);
    assert(argd->global);

    switch(key)
    {
        case ARGP_KEY_ARG:
            argd->physical_topo_filename = arg;
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

void cmd_discover(struct argp_state* state)
{
    struct arg_cmd argd = { 0, 0, 0};
    int    argc = state->argc - state->next + 1;
    char** argv = &state->argv[state->next - 1];
    char*  argv0 =  argv[0];

    argd.global = state->input;

    argv[0] = malloc(strlen(state->name) + strlen(" discover") + 1);

    if(!argv[0])
        argp_failure(state, 1, ENOMEM, 0);

    sprintf(argv[0], "%s discover", state->name);

    argp_parse(&argp_cmd, argc, argv, ARGP_IN_ORDER, &argc, &argd);

    free(argv[0]);

    argv[0] = argv0;

    state->next += argc - 1;

    if (!argd.physical_topo_filename) {
        argp_usage (state);
    }

    discover_and_save_physical_topology(argd.physical_topo_filename);

    return;
}
