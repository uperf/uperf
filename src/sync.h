/* Copyright (C) 2008 Sun Microsystems
 *
 * This file is part of uperf.
 *
 * uperf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.
 *
 * uperf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with uperf.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved. Use is
 * subject to license terms.
 */

#ifndef	_SYNC_H
#define	_SYNC_H

#include <pthread.h>

#define	BARRIER_REACHED(a)		!barrier_notreached((a))
#define	BARRIER_NOTREACHED(a)		barrier_notreached((a))

/* Barrier obj with cond var and mutex lock */
typedef	struct sync_barrier {
	pthread_rwlockattr_t rwattr;
	pthread_rwlock_t barrier;
#ifndef HAVE_ATOMIC_H
	pthread_mutexattr_t count_mtx_attr;
	pthread_mutex_t count_mutex;
#endif /* HAVE_ATOMIC_H */
	volatile unsigned int count;
	volatile unsigned int limit;
	int group;
	int txn;
} barrier_t;

int init_barrier(barrier_t *, int);
int wait_barrier(barrier_t *);
barrier_t * get_barrier(int, int);
int cancel_barrier(barrier_t *);
int barrier_notreached(barrier_t *);
int unlock_barrier(barrier_t *);

#endif /* _SYNC_H */
