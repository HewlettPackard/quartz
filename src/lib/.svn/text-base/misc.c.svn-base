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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#include <stdio.h>
size_t string_to_size(char* str)
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
