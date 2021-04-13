/*
 * Copyright (C) 2008 Sun Microsystems
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
 * along with uperf.  If not, see http://www.gnu.org/licenses/.
 */

/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved. Use is
 * subject to license terms.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */
#ifdef HAVE_ATOMIC_H
#include <atomic.h>
#endif /* HAVE_ATOMIC_H */
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "sync.h"
#include "uperf.h"

int
init_barrier(barrier_t *bar, int threshold)
{
	pthread_rwlockattr_init(&bar->rwattr);
#ifndef STRAND_THREAD_ONLY
	if (pthread_rwlockattr_setpshared(&bar->rwattr,
		PTHREAD_PROCESS_SHARED) != 0) {
		printf("Error in pthread_rwlockattr_setpshared\n");
		exit(1);
	}
#endif /* STRAND_THREAD_ONLY */
	if (pthread_rwlock_init(&bar->barrier, &bar->rwattr) != 0) {
		printf("pthread_rwlock_init failed\n");
		exit(1);
	}

	bar->count = 0;
	bar->limit = threshold;

	if (pthread_rwlock_wrlock(&bar->barrier) != 0) {
		printf("Initial write lock grab failed\n");
		exit(1);
	}
#ifndef HAVE_ATOMIC_H
	pthread_mutexattr_init(&bar->count_mtx_attr);
#ifndef STRAND_THREAD_ONLY
	if (pthread_mutexattr_setpshared(&bar->count_mtx_attr,
		PTHREAD_PROCESS_SHARED) != 0) {
		printf("Error in pthread_mutexattr_setpshared\n");
		exit(1);
	}
#endif /* STRAND_THREAD_ONLY */
	if (pthread_mutex_init(&bar->count_mutex, &bar->count_mtx_attr) != 0) {
		printf("Error in pthread_mutex_init\n");
		exit(1);
	}
#endif /* HAVE_ATOMIC_H */
	return (0);
}

int
unlock_barrier(barrier_t *bar)
{
	return (pthread_rwlock_unlock(&bar->barrier));
}

int
barrier_notreached(barrier_t *bar)
{
	return (bar->limit - bar->count);
}
int
wait_barrier(barrier_t *bar)
{
	assert(bar);
	assert(bar->limit > 0);

#ifdef HAVE_ATOMIC_H
	(void) atomic_add_32_nv(&bar->count, 1);
#else
	if (pthread_mutex_lock(&bar->count_mutex) != 0) {
		printf("Error grabbing count mutex.. aborting\n");
		exit(1);
	}
	bar->count++;
	if (pthread_mutex_unlock(&bar->count_mutex) != 0) {
		printf("Error releasing count mutex.. aborting\n");
		exit(1);
	}
#endif
	if (pthread_rwlock_rdlock(&bar->barrier) == 0) {
		return (pthread_rwlock_unlock(&bar->barrier));
	}

	return (-1);
}
