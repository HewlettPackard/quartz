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

#define _XOPEN_SOURCE

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
#include <utmp.h>
#include <attr/xattr.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <libconfig.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#include "error.h"
#include "cpu/cpu.h"
#include "misc.h"
#include "topology.h"

#include "common.h"

struct arg_cmd
{
    struct arg_global* global;
    char* name;
    int size;
    const char* virtual_topo_cfg_filename;
};

static struct argp_option opt_cmd[] =
{
    { 0 }
};

static char doc_cmd[] =
  "\n"
  "Create virtual topology defined in FILE."
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
            argd->virtual_topo_cfg_filename = arg;
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
    "FILE",
    doc_cmd
};

static int mount_tmpfs(const char* path, size_t size, int membind)
{
    int rc;
    char options[512];

    DBG_LOG(INFO, "Mount tmpfs %s size %zu membind %d\n", path, size, membind);
 
    snprintf(options, 512, "size=%zu,mpol=bind:%d", size, membind);
   
    if (!(rc = mount("tmpfs", path, "tmpfs", 0, options))) {
        return E_ERROR;
    }

    return E_SUCCESS; 
}

static void create_nvm(virtual_topology_element_t* vte, void* arg)
{
    assert(vte);
    assert(vte->element);

    virtual_nvm_t* v_nvm = vte->element;

    const char* path = v_nvm->mountpath;
    size_t size = v_nvm->size;
    int membind = v_nvm->membind;

    DBG_LOG(INFO, "Create nvm %s size %zu membind %d\n", path, size, membind);

    if (mkdir_recursive(path, S_IRWXU) < 0) {
        DBG_LOG(ERROR, "%s\n", strerror(errno));
        return;
    }

    mount_tmpfs(path, size, membind);
}

void cmd_create(struct argp_state* state)
{
    struct arg_cmd argd = { 0, 0, 0};
    int    argc = state->argc - state->next + 1;
    char** argv = &state->argv[state->next - 1];
    char*  argv0 =  argv[0];

    argd.global = state->input;

    argv[0] = malloc(strlen(state->name) + strlen(" create") + 1);

    if(!argv[0])
        argp_failure(state, 1, ENOMEM, 0);

    sprintf(argv[0], "%s create", state->name);

    argp_parse(&argp_cmd, argc, argv, ARGP_IN_ORDER, &argc, &argd);

    free(argv[0]);

    argv[0] = argv0;

    state->next += argc - 1;

    if (!argd.virtual_topo_cfg_filename) {
        argp_usage (state);
    }

    check_running_as_root();

    config_t cfg;
    config_init(&cfg);

    if (!config_read_file(&cfg, argd.virtual_topo_cfg_filename)) {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
                        config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return;
    }

    physical_topology_t* pt;
    virtual_topology_t* vt;

    load_physical_topology(&cfg, &pt);
    create_virtual_topology(&cfg, pt, &vt);
    crawl_virtual_topology(vt, NULL, NULL, create_nvm, NULL);
    destroy_virtual_topology(vt);

    return;
}
