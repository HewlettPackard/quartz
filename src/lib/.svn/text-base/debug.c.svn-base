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
#include "debug.h"
#include <sys/types.h>
#include <execinfo.h>
#include <unistd.h>
#include <stdio.h>
#include "config.h"


int         dbg_modules[dbg_module_count];
int         dbg_level = 0;
int         dbg_verbose = 0;
const char* dbg_identifier = "";
static char dbg_identifier_buf[128];

static int 
strrep(char *target, char *source, char oldc, char newc)
{
	int i;

	for (i=0; source[i]; i++) {
		if (source[i] == oldc) {
			target[i] = newc;
		} else {
			target[i] = source[i];
		}
	}
	target[i] = '\0';
	return 0;
}


void
dbg_set_level(int level)
{
	dbg_level = level;
}


int 
dbg_init(config_t* dbg_cfg, int level, const char* identifier)
{
	// if user hasn't provided a debugging level then get it from the 
	// configuration env/file
	if (level < 0) {
		__cconfig_lookup_int(dbg_cfg, "debug.level", &dbg_level);
	} else {
		dbg_level = level;
	}

	__cconfig_lookup_int(dbg_cfg, "debug.verbose", &dbg_verbose);

	// if user hasn't provide an identifier then check whether the environment 
	// provides one, othewise create one based on process' pid 
	if (!identifier) {
		dbg_identifier = getenv("DEBUG_IDENTIFIER");
		if (!dbg_identifier) {
			sprintf(dbg_identifier_buf, "%d", getpid()); 
			dbg_identifier = dbg_identifier_buf;
		}
	} else {
		dbg_identifier = identifier;
	}


	// read per module debugging flags
#define STR(name) #name
#define ACTION(name)                                                           \
	do {                                                                       \
		char dotstr[128];                                                      \
		strrep(dotstr, STR(debug_module_##name), '_', '.');                    \
		__cconfig_lookup_bool(dbg_cfg, dotstr,                                 \
		                      &dbg_modules[dbg_module_##name]);                \
	} while (0);

	FOREACH_DEBUG_MODULE(ACTION)
#undef ACTION
        DBG_LOG(DEBUG, ""); // prevent compiler warning
	return 0;
}


void
dbg_backtrace (void)
{
	void *array[10];
	size_t size;
	char **strings;
	size_t i;
 
	size = backtrace (array, 10);
	strings = backtrace_symbols (array, size);
									      
	printf ("Obtained %zd stack frames.\n", size);
																	      
	for (i = 0; i < size; i++)
		printf ("%s\n", strings[i]);
	free (strings);
}
