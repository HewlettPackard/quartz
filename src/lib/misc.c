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

#include <ctype.h>
#include <dirent.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>


size_t string_to_size(const char* str)
{
    size_t factor = 1;
    size_t size;
    long   val;
    char*  endptr = 0;

    val = strtoull(str, &endptr, 10);
    while(endptr && (endptr - str) < strlen(str) && !isalpha(*endptr)) {endptr++;}

    switch (endptr[0]) {
        case 'K': case 'k':
            factor = 1024LLU;
            break;
        case 'M': case 'm':
            factor = 1024LLU*1024LLU;
            break;
        case 'G': case 'g':
            factor = 1024LLU*1024LLU*1024LLU;
            break;
        default:
            factor = 1;
    }
    size = factor * val;
    return size;
}

int string_prefix(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

static int direxists(const char* path)
{
    DIR* dir = opendir(path);
    if (dir) {
        closedir(dir);
        return 1;
    }
    else if (ENOENT == errno)
    {
        return 0;
    }
    /* opendir failed for some other reason */
    return -1;
}

int mkdir_recursive(const char *dir, mode_t mode) {
    char tmp[256];
    char *p = NULL;
    size_t len;
    int ret;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);
    if(tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            if (direxists(tmp) == 0) {
                mkdir(tmp, mode);
            }
            *p = '/';
        }
    }
    if (direxists(tmp) == 0) {
        if ((ret = mkdir(tmp, mode)) != 0) {
            return ret;
        }
    }
    return 0;
}


