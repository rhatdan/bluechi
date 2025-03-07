/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>

#include "libhirte/common/opt.h"
#include "libhirte/common/parse-util.h"
#include "libhirte/service/shutdown.h"

#include "manager.h"

const struct option options[] = { { ARG_PORT, required_argument, 0, ARG_PORT_SHORT },
                                  { ARG_CONFIG, required_argument, 0, ARG_CONFIG_SHORT },
                                  { ARG_HELP, no_argument, 0, ARG_HELP_SHORT },
                                  { NULL, 0, 0, '\0' } };

#define OPTIONS_STR ARG_PORT_SHORT_S ARG_HELP_SHORT_S ARG_CONFIG_SHORT_S

static const char *opt_port = 0;
static const char *opt_config = NULL;

static void usage(char *argv[]) {
        printf("Usage:\n"
               "\t%s [options...] \n"
               "Available options are:\n"
               "\t-%c %s\t\t Print this help message.\n"
               "\t-%c %s\t\t The port of hirte to connect to.\n"
               "\t-%c %s\t A path to a config file used to bootstrap hirte-agent.\n",
               argv[0],
               ARG_HELP_SHORT,
               ARG_HELP,
               ARG_PORT_SHORT,
               ARG_PORT,
               ARG_CONFIG_SHORT,
               ARG_CONFIG);
}

static int get_opts(int argc, char *argv[]) {
        int opt = 0;

        while ((opt = getopt_long(argc, argv, OPTIONS_STR, options, NULL)) != -1) {
                switch (opt) {
                case ARG_HELP_SHORT:
                        usage(argv);
                        return 1;

                case ARG_PORT_SHORT:
                        opt_port = optarg;
                        break;

                case ARG_CONFIG_SHORT:
                        opt_config = optarg;
                        break;

                default:
                        fprintf(stderr, "Unsupported option %c\n", opt);
                        usage(argv);
                        return -EINVAL;
                }
        }

        return 0;
}


int main(int argc, char *argv[]) {
        int r = get_opts(argc, argv);
        if (r < 0) {
                return EXIT_FAILURE;
        } else if (r > 0) {
                return EXIT_SUCCESS;
        }

        _cleanup_manager_ Manager *manager = manager_new();
        if (manager == NULL) {
                return EXIT_FAILURE;
        }

        /* First load config */
        if (!manager_parse_config(manager, opt_config)) {
                return EXIT_FAILURE;
        }

        /* Then override individual options */

        if (opt_port && !manager_set_port(manager, opt_port)) {
                return EXIT_FAILURE;
        }

        if (manager_start(manager)) {
                return EXIT_SUCCESS;
        }
        return EXIT_FAILURE;
}
