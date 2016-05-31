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
#ifndef __INTERPOSE_H
#define __INTERPOSE_H


/**
 * 
 * \page library_interposition Library interposition 
 * 
 * The emulator intercepts several events of interest. It achieves this
 * by interposing on corresponding functions. 
 * Currently this includes thread creation and POSIX synchronization mechanisms.
 */

extern int (*__lib_pthread_create)(pthread_t *thread, const pthread_attr_t *attr,
                                   void *(*start_routine) (void *), void *arg);
extern int (*__lib_pthread_mutex_lock)(pthread_mutex_t *mutex);
extern int (*__lib_pthread_mutex_trylock)(pthread_mutex_t *mutex);
extern int (*__lib_pthread_mutex_unlock)(pthread_mutex_t *mutex);
extern int (*__lib_pthread_detach)(pthread_t thread);

int init_interposition();

#endif /* __INTERPOSE_H */
