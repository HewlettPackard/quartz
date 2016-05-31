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

#include <stdio.h>
#include <unistd.h>


#define BUF_SIZE (2048)

unsigned long mem[BUF_SIZE][BUF_SIZE];

void iter()
{
	int i;
	int j;
	unsigned long k;

	for (i=0; i < BUF_SIZE; ++i) {
		for (j=0; j < BUF_SIZE; ++j) {
			mem[i][j] = i * j;
		}
	}

	k = 0;
	while(1) {
		for (i=0; i < BUF_SIZE; ++i) {
			__asm__ __volatile__("");
			for (j=0; j < BUF_SIZE; ++j) {
		        k += mem[j][i] + i*j;
		        mem[j][i] = k;
			}
		}
//		fprintf(stdout, "k is %lu\n", (unsigned long)k);
		usleep(1000);
	}
}

int main()
{
    iter();
    return 0;
}
