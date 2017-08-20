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
    { "physical", 'p', "FILE", 0,
        "Physical topology xml file." },

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
        case 'c':
            argd->virtual_topo_cfg_filename = arg;
            break;
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

void cmd_destroy(struct argp_state* state)
{
    struct arg_cmd argd = { 0, 0, 0};
    int    argc = state->argc - state->next + 1;
    char** argv = &state->argv[state->next - 1];
    char*  argv0 =  argv[0];

    argd.global = state->input;

    argv[0] = malloc(strlen(state->name) + strlen(" destroy") + 1);

    if(!argv[0])
        argp_failure(state, 1, ENOMEM, 0);

    sprintf(argv[0], "%s destroy", state->name);

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

    cpu_model_t* cpu;
    if ((cpu = cpu_model()) == NULL) {
        printf("wrong model\n");
    }

    destroy_virtual_topology(&cfg);    

    return;
}
