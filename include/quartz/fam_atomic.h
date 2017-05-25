/*
 * Copyright © 2015, Hewlett Packard Enterprise Development LP
 *
 * Author: Jason Low <jason.low2@hpe.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifndef _FAM_ATOMIC_H_
#define _FAM_ATOMIC_H_

#include <stdint.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/types.h>

/*
 * The libfam-atomic library is compiled in C, so when C++ applications
 * use the fam atomic interfaces, the compiler needs to be notified
 * that the fam atomic functions are compiled in C. We'll specify the
 * extern "C" in the header file so that C++ applications don't need
 * to worry about this.
 */
#ifdef __cplusplus
extern "C" {
#endif

/* Fabric Attached Memory Atomic ioctl interface */

struct fam_atomic_args_32 {
        int lfs_fd;
        int64_t offset;
        int32_t p32_0, p32_1;
};

struct fam_atomic_args_64 {
        int lfs_fd;
        int64_t offset;
        int64_t p64_0, p64_1;
};

struct fam_atomic_args_128 {
	int lfs_fd;
        int64_t offset;
        int64_t p128_0[2], p128_1[2];
};

#define _FAM_ATOMIC_MAGIC                       0xaa
#define _FAM_ATOMIC_32_FETCH_AND_ADD_NR         0x11
#define _FAM_ATOMIC_64_FETCH_AND_ADD_NR         0x12
#define _FAM_ATOMIC_32_SWAP_NR                  0x21
#define _FAM_ATOMIC_64_SWAP_NR                  0x22
#define _FAM_ATOMIC_128_SWAP_NR                 0x23
#define _FAM_ATOMIC_32_COMPARE_AND_STORE_NR     0x31
#define _FAM_ATOMIC_64_COMPARE_AND_STORE_NR     0x32
#define _FAM_ATOMIC_128_COMPARE_AND_STORE_NR    0x33
#define _FAM_ATOMIC_128_READ_NR                 0x43
#define _FAM_ATOMIC_WR(nr,type)                 _IOWR(_FAM_ATOMIC_MAGIC, (nr), type)
#define _FAM_ATOMIC_R(nr,type)                  _IOR(_FAM_ATOMIC_MAGIC, (nr), type)

/* temp = *offset; *offset += p0; p0 = temp */
#define FAM_ATOMIC_32_FETCH_AND_ADD             _FAM_ATOMIC_WR(_FAM_ATOMIC_32_FETCH_AND_ADD_NR, struct fam_atomic_args_32)
#define FAM_ATOMIC_64_FETCH_AND_ADD             _FAM_ATOMIC_WR(_FAM_ATOMIC_64_FETCH_AND_ADD_NR, struct fam_atomic_args_64)

/* temp = *offset; *offset = p0; p0 = temp; */
#define FAM_ATOMIC_32_SWAP                      _FAM_ATOMIC_WR(_FAM_ATOMIC_32_SWAP_NR, struct fam_atomic_args_32)
#define FAM_ATOMIC_64_SWAP                      _FAM_ATOMIC_WR(_FAM_ATOMIC_64_SWAP_NR, struct fam_atomic_args_64)
#define FAM_ATOMIC_128_SWAP                     _FAM_ATOMIC_WR(_FAM_ATOMIC_128_SWAP_NR, struct fam_atomic_args_128)

/* temp = *offset; if (temp == p0) *offset = p1; p0 = temp; */
#define FAM_ATOMIC_32_COMPARE_AND_STORE         _FAM_ATOMIC_WR(_FAM_ATOMIC_32_COMPARE_AND_STORE_NR, struct fam_atomic_args_32)
#define FAM_ATOMIC_64_COMPARE_AND_STORE         _FAM_ATOMIC_WR(_FAM_ATOMIC_64_COMPARE_AND_STORE_NR, struct fam_atomic_args_64)
#define FAM_ATOMIC_128_COMPARE_AND_STORE        _FAM_ATOMIC_WR(_FAM_ATOMIC_128_COMPARE_AND_STORE_NR, struct fam_atomic_args_128)

/* p0 = *offset */
#define FAM_ATOMIC_128_READ                     _FAM_ATOMIC_R(_FAM_ATOMIC_128_READ_NR, struct fam_atomic_args_128)

/*
 * Register Fabric Attached Memory regions.
 *
 * NVM regions containing fam atomics and/or fam locks must be registered
 * before any of the atomics or locks within the region can be used.
 *
 * @region_start: Address of the start of the NVM region.
 * @region_length: The length of the NVM region.
 * @fd: The file descriptor associated with the open NVM region.
 * @offset: The offset from the start of the file.
 *	    (If "region_start", is at the start of the file, then offset is 0.
 *
 * Return: 0 if the register function succeeds, else a negative value.
 */
int
fam_atomic_register_region(void *region_start,
			   size_t region_length,
			   int fd,
			   off_t offset);

void
fam_atomic_unregister_region(void *region_start,
			     size_t region_length);

/*
 * Fabric Attached Memory Atomic API.
 *
 * Users of fam atomics must manually keep the fam atomic variables in
 * their own cachelines so that they do not share cachelines with regular
 * data. This is required to ensure the correctness of the atomic data.
 */

/*
 * Atomically adds "increment" to the atomic variable and returns the
 * previous value of the atomic variable.
 *
 * @address: Pointer to an fam atomic variable.
 * @increment: The value which will be added to the atomic.
 */
int32_t
fam_atomic_32_fetch_add(int32_t *address,
			int32_t increment);

int64_t
fam_atomic_64_fetch_add(int64_t *address,
			int64_t increment);

/*
 * Atomically writes "value" to the atomic variable and returns
 * the previous value of the atomic variable.
 *
 * @address: Pointer to an fam atomic variable.
 * @value: The new value that will be written to the atomic.
 */
int32_t
fam_atomic_32_swap(int32_t *address,
		   int32_t value);

int64_t
fam_atomic_64_swap(int64_t *address,
		   int64_t value);

void
fam_atomic_128_swap(int64_t *address,
		    int64_t value[2],
		    int64_t result[2]);

/*
 * Atomically checks if the atomic variable is equal to "compare"
 * and sets the atomic to "store" if true. Returns the previous
 * value of the atomic varible.
 *
 * @address: Pointer to an fam atomic variable.
 * @compare The value which the atomic is expected to equal.
 * @store: The value the atomic will be set to if equal to "compare".
 */
int32_t
fam_atomic_32_compare_store(int32_t *address,
			    int32_t compare,
			    int32_t store);

int64_t
fam_atomic_64_compare_store(int64_t *address,
			    int64_t compare,
			    int64_t store);

void
fam_atomic_128_compare_store(int64_t *address,
			     int64_t compare[2],
			     int64_t store[2],
			     int64_t result[2]);

/*
 * Returns the value of the atomic variable.
 *
 * @address: Pointer to an fam atomic variable.
 */
int32_t
fam_atomic_32_read(int32_t *address);

int64_t
fam_atomic_64_read(int64_t *address);

void
fam_atomic_128_read(int64_t *address,
		    int64_t result[2]);

/*
 * Writes "value" to the atomic variable.
 *
 * @address: Pointer to an fam atomic variable.
 * @value: The value that will be written to the atomic.
 */
void
fam_atomic_32_write(int32_t *address,
		    int32_t value);

void
fam_atomic_64_write(int64_t *address,
		    int64_t value);

void
fam_atomic_128_write(int64_t *address,
		     int64_t value[2]);

/*
 * Atomically performs bitwise AND between the fam atomic
 * variable and 'arg'.
 *
 * @address: Pointer to an fam atomic variable.
 * @arg: The value that the atomic gets AND'd with.
 */
int32_t
fam_atomic_32_fetch_and(int32_t *address,
			int32_t arg);

int64_t
fam_atomic_64_fetch_and(int64_t *address,
			int64_t arg);

/*
 * Atomically performs bitwise OR between the fam atomic
 * variable and 'arg'.
 *
 * @address: Pointer to an fam atomic variable.
 * @arg: The value that the atomic gets OR'd with.
 */
int32_t
fam_atomic_32_fetch_or(int32_t *address,
		       int32_t arg);

int64_t
fam_atomic_64_fetch_or(int64_t *address,
		       int64_t arg);

/*
 * Atomically performs bitwise XOR between the fam atomic
 * variable and 'arg'.
 *
 * @address: Pointer to an fam atomic variable.
 * @arg: The value that the atomic gets XOR'd with.
 */
int32_t
fam_atomic_32_fetch_xor(int32_t *address,
			int32_t arg);

int64_t
fam_atomic_64_fetch_xor(int64_t *address,
			int64_t arg);

/* Spinlocks */
typedef int32_t	__fam_ticket_t;
typedef int64_t	__fam_ticketpair_t;

struct __fam_tickets {
	__fam_ticket_t  head;   /* low bytes */
	__fam_ticket_t  tail;   /* high bytes */
};

/*
 * The spinlock is a queue made from two values, head and tail. To
 * lock, you increment tail and then wait until head reaches the
 * previous tail value. This makes the queuing "fair", in that tasks
 * arriving at the spinlock earlier get to run sooner.
 *
 * The increment has to be done atomically so that only one task is
 * waiting for head to reach each unique tail value
 *
 * By laying out the head and tail in sequential memory and then
 * aliasing that to a value of twice the width, we can actually
 * increment the tail value and fetch the head in a single operation.
 * We place the tail in the high order bytes and so that when we add
 * to it, the result won't overflow into the head value. This is a
 * cute trick cribbed from the Linux spinlock code.
 */
struct fam_spinlock {
	union {
		__fam_ticketpair_t      head_tail;
		struct __fam_tickets	tickets;
	};
};

#define FAM_SPINLOCK_INITIALIZER ((struct fam_spinlock) { 0 })

extern void
fam_spin_lock_init(struct fam_spinlock *lock);

extern void
fam_spin_lock(struct fam_spinlock *lock);

extern bool
fam_spin_trylock(struct fam_spinlock *lock);

extern void
fam_spin_unlock(struct fam_spinlock *lock);

/*
 * Deprecated unpadded APIs.
 */
static inline int32_t
fam_atomic_32_fetch_and_add_unpadded(int32_t *address,
				     int32_t increment) __attribute__ ((deprecated));

static inline int64_t
fam_atomic_64_fetch_and_add_unpadded(int64_t *address,
				     int64_t increment) __attribute__ ((deprecated));

static inline int32_t
fam_atomic_32_swap_unpadded(int32_t *address,
			    int32_t value) __attribute__ ((deprecated));

static inline int64_t
fam_atomic_64_swap_unpadded(int64_t *address,
			    int64_t value) __attribute__ ((deprecated));

static inline void
fam_atomic_128_swap_unpadded(int64_t *address,
			     int64_t value[2],
			     int64_t result[2]) __attribute__ ((deprecated));

static inline int32_t
fam_atomic_32_compare_and_store_unpadded(int32_t *address,
					 int32_t compare,
					 int32_t store) __attribute__ ((deprecated));

static inline int64_t
fam_atomic_64_compare_and_store_unpadded(int64_t *address,
					 int64_t compare,
					 int64_t store) __attribute__ ((deprecated));

static inline void
fam_atomic_128_compare_and_store_unpadded(int64_t *address,
					  int64_t compare[2],
					  int64_t store[2],
					  int64_t result[2]) __attribute__ ((deprecated));

static inline int32_t
fam_atomic_32_read_unpadded(int32_t *address) __attribute__ ((deprecated));

static inline int64_t
fam_atomic_64_read_unpadded(int64_t *address) __attribute__ ((deprecated));

static inline void
fam_atomic_128_read_unpadded(int64_t *address,
			     int64_t result[2]) __attribute__ ((deprecated));

static inline void
fam_atomic_32_write_unpadded(int32_t *address,
			     int32_t value) __attribute__ ((deprecated));

static inline void
fam_atomic_64_write_unpadded(int64_t *address,
			     int64_t value) __attribute__ ((deprecated));

static inline void
fam_atomic_128_write_unpadded(int64_t *address,
			      int64_t value[2]) __attribute__ ((deprecated));

static inline int32_t
fam_atomic_32_fetch_and_unpadded(int32_t *address,
				 int32_t arg) __attribute__ ((deprecated));

static inline int64_t
fam_atomic_64_fetch_and_unpadded(int64_t *address,
				 int64_t arg) __attribute__ ((deprecated));

static inline int32_t
fam_atomic_32_fetch_or_unpadded(int32_t *address,
				int32_t arg) __attribute__ ((deprecated));

static inline int64_t
fam_atomic_64_fetch_or_unpadded(int64_t *address,
				int64_t arg) __attribute__ ((deprecated));

static inline int32_t
fam_atomic_32_fetch_xor_unpadded(int32_t *address,
				 int32_t arg) __attribute__ ((deprecated));

static inline int64_t
fam_atomic_64_fetch_xor_unpadded(int64_t *address,
				 int64_t arg) __attribute__ ((deprecated));

static inline int32_t
fam_atomic_32_fetch_and_add_unpadded(int32_t *address,
				     int32_t increment)
{
	return fam_atomic_32_fetch_add(address, increment);
}

static inline int64_t
fam_atomic_64_fetch_and_add_unpadded(int64_t *address,
				     int64_t increment)
{
	return fam_atomic_64_fetch_add(address, increment);
}

static inline int32_t
fam_atomic_32_swap_unpadded(int32_t *address,
			    int32_t value)
{
	return fam_atomic_32_swap(address, value);
}

static inline int64_t
fam_atomic_64_swap_unpadded(int64_t *address,
			    int64_t value)
{
	return fam_atomic_64_swap(address, value);
}

static inline void
fam_atomic_128_swap_unpadded(int64_t *address,
			     int64_t value[2],
			     int64_t result[2])
{
	fam_atomic_128_swap(address, value, result);
}

static inline int32_t
fam_atomic_32_compare_and_store_unpadded(int32_t *address,
					 int32_t compare,
					 int32_t store)
{
	return fam_atomic_32_compare_store(address, compare, store);
}

static inline int64_t
fam_atomic_64_compare_and_store_unpadded(int64_t *address,
					 int64_t compare,
					 int64_t store)
{
	return fam_atomic_64_compare_store(address, compare, store);
}

static inline void
fam_atomic_128_compare_and_store_unpadded(int64_t *address,
					  int64_t compare[2],
					  int64_t store[2],
					  int64_t result[2])
{
	fam_atomic_128_compare_store(address, compare, store, result);
}

static inline int32_t
fam_atomic_32_read_unpadded(int32_t *address)
{
	return fam_atomic_32_read(address);
}

static inline int64_t
fam_atomic_64_read_unpadded(int64_t *address)
{
	return fam_atomic_64_read(address);
}

static inline void
fam_atomic_128_read_unpadded(int64_t *address,
			     int64_t result[2])
{
	fam_atomic_128_read(address, result);
}

static inline void
fam_atomic_32_write_unpadded(int32_t *address,
			     int32_t value)
{
	fam_atomic_32_write(address, value);
}

static inline void
fam_atomic_64_write_unpadded(int64_t *address,
			     int64_t value)
{
	fam_atomic_64_write(address, value);
}

static inline void
fam_atomic_128_write_unpadded(int64_t *address,
			      int64_t value[2])
{
	fam_atomic_128_write(address, value);
}

static inline int32_t
fam_atomic_32_fetch_and_unpadded(int32_t *address,
				 int32_t arg)
{
	return fam_atomic_32_fetch_and(address, arg);
}

static inline int64_t
fam_atomic_64_fetch_and_unpadded(int64_t *address,
				 int64_t arg)
{
	return fam_atomic_64_fetch_and(address, arg);
}

static inline int32_t
fam_atomic_32_fetch_or_unpadded(int32_t *address,
				int32_t arg)
{
	return fam_atomic_32_fetch_or(address, arg);
}

static inline int64_t
fam_atomic_64_fetch_or_unpadded(int64_t *address,
				int64_t arg)
{
	return fam_atomic_64_fetch_or(address, arg);
}

static inline int32_t
fam_atomic_32_fetch_xor_unpadded(int32_t *address,
				 int32_t arg)
{
	return fam_atomic_32_fetch_xor(address, arg);
}

static inline int64_t
fam_atomic_64_fetch_xor_unpadded(int64_t *address,
				 int64_t arg)
{
	return fam_atomic_64_fetch_xor(address, arg);
}

#ifdef __cplusplus
}
#endif

#endif /* _FAM_ATOMIC_H_ */
