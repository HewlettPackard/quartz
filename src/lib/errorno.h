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
#ifndef __ERRNO_H
#define __ERRNO_H

#ifdef __DEFINE_ERRNO
# error "__DEFINE_ERRNO previously defined"
#endif

/*
 * Define error codes and error messages here
 */
#define __DEFINE_ERRNO(ACTION)                                               \
	ACTION(E_SUCCESS, "Success")                                             \
	ACTION(E_ERROR, "Generic error")                                         \
	ACTION(E_NOMEM, "No memory")                                             \
    ACTION(E_EXIST, "Name already exists")                                   \
    ACTION(E_NOENT, "Name does not exist")                                   \
    ACTION(E_INVAL, "Invalid argument")                                      \
    ACTION(E_BUSY, "Resource busy")                                          \
    ACTION(E_NOTEMPTY, "Not empty")                                          \
    ACTION(E_ERRNO, "Standard C library error; check errno for details")


#ifdef __ENUM_MEMBER
# error "__ENUM_MEMBER previously defined"
#endif

#define __ENUM_MEMBER(name, str)  name,

enum {
	__DEFINE_ERRNO(__ENUM_MEMBER)
	E_MAXERRNO
};

#undef __ENUM_MEMBER /* don't polute the macro namespace */

#ifdef __ERRNO_STRING
# error "__ERRNO_STRING previously defined"
#endif

#define __ERRNO_STRING(name, str) str,

/*
    TODO: not used for now
static const char* 
ErrorToString(int err) {
	static const char* errstr[] = {
		__DEFINE_ERRNO(__ERRNO_STRING)
		"Unknown error code"
	};
	if (err >= 0 && err < E_MAXERRNO) {
		return errstr[err];
	}
	return errstr[E_MAXERRNO];
}
*/
#undef __ERRNO_STRING /* don't polute the macro namespace */
#undef __DEFINE_ERRNO /* don't polute the macro namespace */

#endif /* __ERRNO_H */
