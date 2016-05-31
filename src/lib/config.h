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
#ifndef __CONFIG_H
#define __CONFIG_H

/**
 * \file 
 * 
 * Runtime configuration parameters
 */


#include <stdio.h>
#include <libconfig.h>

#define ENVVAR_PREFIX "NVMEMUL"

#ifdef __cplusplus
extern "C" {
#endif

/* Make sure we don't redefine a macro already defined in libconfig.h */

#ifdef CONFIG_NO_CHECK
# error "ERROR: Redefining previously defined CONFIG_NO_CHECK"
#else
# define CONFIG_NO_CHECK    0
#endif

#ifdef CONFIG_RANGE_CHECK
# error "ERROR: Redefining previously defined CONFIG_RANGE_CHECK"
#else
# define CONFIG_RANGE_CHECK 1
#endif

#ifdef CONFIG_LIST_CHECK
# error "ERROR: Redefining previously defined CONFIG_LIST_CHECK"
#else
# define CONFIG_LIST_CHECK  2
#endif



/** 
 * The lookup functions return the value of a configuration variable based on 
 * the following order: 
 *  1) value of environment variable
 *  2) value in configuration file variable
 *  
 * If the variable is not found then a lookup function does not set the value.
 */

int __cconfig_lookup_bool(config_t *cfg, const char *name, int *value);
int __cconfig_lookup_int(config_t *cfg, const char *name, int *value);
int __cconfig_lookup_string(config_t *cfg, const char *name, char **value);
int __cconfig_lookup_valid_bool(config_t *cfg, const char *name, int *value, int validity_check, ...);
int __cconfig_lookup_valid_int(config_t *cfg, const char *name, int *value, int validity_check, ...);
int __cconfig_lookup_valid_string(config_t *cfg, const char *name, char **value, int validity_check, ...);
int __cconfig_init(config_t *cfg, const char *config_file);

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_H */
