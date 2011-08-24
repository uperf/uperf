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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/mman.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>
#include <strings.h>
#ifdef HAVE_ATOMIC_H
#include <atomic.h>
#endif /* HAVE_ATOMIC_H */
#include <sys/types.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "uperf.h"

#include "protocol.h"
#include "sync.h"
#include "logging.h"
#include "flowops.h"
#include "workorder.h"
#include "strand.h"
#include "shm.h"

#define	ONE_SEC			1.0e+9
#define	CALLOUT_GRANURALITY	ONE_SEC


#ifdef USE_SHMGET
static int shmid;
#endif /* USE_SHMGET */

uperf_shm_t *global_shm;

/* Get strand_t structure for a strand with id "id" */
strand_t *
shm_get_strand(uperf_shm_t *shm, int id)
{
	return (&shm->strands[id]);
}

/*
 * Register a callout for time "stop". At "stop" time, strands
 * belonging to group_id will be "called" (signalled) out
 * No need for locks as only one strand per group will register
 */
int
shm_callout_register(uperf_shm_t *shm, hrtime_t stop, int group_id)
{
	if (shm->callouts[group_id] == 0) {
		shm->callouts[group_id] = (hrtime_t) stop/CALLOUT_GRANURALITY;
		return (0);
	}
	/* Callout already registered!. Should not happen */
	return (-1);
}

/*
 * Returns 0 if no callouts
 * 	-1 if error
 * 	x if called out
 */
int
shm_process_callouts(uperf_shm_t *shm)
{
	int i;
	int no_groups;
	int called_out = 0;
	hrtime_t now;

	if (shm->role == MASTER)
		no_groups = shm->workorder->ngrp;
	else
		no_groups = 1;

	now = (hrtime_t) GETHRTIME()/CALLOUT_GRANURALITY;
	for (i = 0; i < no_groups; i++) {
		hrtime_t timeout = shm->callouts[i];

		if (timeout > 0 && timeout <= now) {
			if (signal_all_strands(shm, i, SIGUSR2) != 0) {
				shm->global_error++;
				uperf_log_msg(UPERF_LOG_ERROR, 0,
				    "Error signalling strands");
				return (-1);
			}
			shm->callouts[i] = 0;
			called_out++;
			uperf_info("called out\n");
		}
	}

	return (called_out);
}
/*
 * Initialize and prime all barriers for the master.
 * shm->bar[i] is used with "i"th transaction.
 * Returns 0 on success, -1 on error
 */
int
shm_init_barriers_master(uperf_shm_t *shm, workorder_t *w)
{
	int n = 0;
	int i;
	int strand_per_txn[NUM_BARRIER];

	(void) memset(strand_per_txn, 0, sizeof (strand_per_txn));
	for (i = 0; i < w->ngrp; i++) {
		int j;
		group_t *g = &w->grp[i];

		n = MAX(n, g->ntxn);
		for (j = 0; j < g->ntxn; j++) {
			strand_per_txn[j] += g->nthreads;
		}
	}

	if (n > NUM_BARRIER) {
		uperf_error("Shm exhausted!\n");
		return (-1);
	}
	shm->nobarrier = n;

	/* master will also participate in barrier */
	for (i = 0; i < n; i++) {
		init_barrier(&shm->bar[i], strand_per_txn[i]);
	}

	return (0);
}

/* Initialize and prime barriers for the slave */
int
shm_init_barriers_slave(uperf_shm_t *shm, group_t *g)
{
	int n;
	int i;

	n = g->ntxn;
	if (n > NUM_BARRIER) {
		uperf_error("Shm exhausted!\n");
		return (-1);
	}

	for (i = 0; i < n; i++) {
		init_barrier(&shm->bar[i], g->nthreads);
	}
	shm->nobarrier = n;

	return (0);
}

/* ARGSUSED1 */
barrier_t *
shm_get_barrier(uperf_shm_t *shm, int grp, int txn)
{
	return (&shm->bar[txn]);
}


/*
 * Initialize the shared region. We have to use either shmat(2) or
 * mmap(2) with MAP_SHARED flag as we support processes too.
 */
uperf_shm_t *
shm_init(workorder_t *w)
{
	int no_strands;
	size_t shm_size;
	size_t strand_size;
	size_t connlist_size;
	uperf_shm_t *shared;

	no_strands = workorder_num_strands(w);
	strand_size = workorder_num_strands(w) * sizeof (strand_t);
	connlist_size = workorder_num_connections(w) * sizeof (protocol_t *);

	shm_size = strand_size + connlist_size + sizeof (uperf_shm_t);
	uperf_info("Allocating shared memory of size %d bytes\n", shm_size);

#ifdef USE_SHMGET
	shmid = shmget(0, shm_size, IPC_CREAT | 0666);
	if (shmid == -1) {
		char msg[1024];
		(void) snprintf(msg, sizeof (1024), "shmget for size %d failed",
		    shm_size);
		perror(msg);
		return (NULL);
	}
	shared = (void *) shmat(shmid, (void *) 0, SHM_SHARE_MMU);
	if (shared == (void *)-1) {
		perror("shmat");
		return (NULL);
	}
#else
	shared = (void *) mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
	    MAP_SHARED | MAP_ANON, -1, 0);
	if (shared == (void *)-1) {
		char msg[1024];
		(void) snprintf(msg, sizeof (msg), "mmap for size %lu failed",
		    (unsigned long)shm_size);
		perror(msg);
		return (NULL);
	}
#endif /* USE_SHMGET */

	bzero(shared, shm_size);
	shared->init = 1;
	shared->size = shm_size;
	shared->no_strands = no_strands;
	shared->connection_list = (protocol_t **)((char *)shared
	    + sizeof (uperf_shm_t) + strand_size);

	shared->nstats_size = no_strands * workorder_num_stats(w)
	    * sizeof (newstats_t);
	shared->nstats = (newstats_t *) mmap(NULL, shared->nstats_size,
	    PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);

	uperf_log_init(&shared->log);

	if (pthread_mutexattr_init(&shared->attr) != 0) {
		perror("pthread_mutexattr_init");
		shm_fini(shared);
		return (NULL);
	}
#ifndef STRAND_THREAD_ONLY
	if (pthread_mutexattr_setpshared(&shared->attr,
		PTHREAD_PROCESS_SHARED) != 0) {
		perror("pthread_mutexattr_setpshared");
		shm_fini(shared);
		return (NULL);
	}
#endif /* STRAND_THREAD_ONLY */
	if (pthread_mutex_init(&shared->lock, &shared->attr) != 0) {
		perror("pthread_mutex_init");
		shm_fini(shared);
		return (NULL);
	}
	strlcpy(shared->agg_stat.name, AGG_STAT_NAME, UPERF_NAME_LEN);
	global_shm = shared;

	return (shared);
}

void
shm_fini(uperf_shm_t *shm)
{
	if (shm != NULL) {
		uperf_error("\n");
		uperf_log_flush();
#ifdef USE_SHMGET
		shmdt((char *) shm);
		if (shmctl(shmid, IPC_RMID, 0) == -1) {
			/* ignore */
		}
#else
		(void) munmap((void *)shm->nstats, shm->nstats_size);
		(void) munmap((void *)shm, shm->size);
#endif
	}
}

void
shm_update_strand_exit(uperf_shm_t *shm)
{
#ifdef HAVE_ATOMIC_H
	atomic_add_32(&shm->finished, 1);
#else
	shm->finished++;
#endif /* HAVE_ATOMIC_H */
}

void
flag_error(char *reason)
{
	global_shm->global_error++;
	if (reason)
		uperf_info("%s:%s\n", __func__, reason);
}

newstats_t *
malloc_newstats(uperf_shm_t *shm, stats_type_t type, int sid,
    int gid, int tid, int fid, char *name)
{
	newstats_t *ns = &shm->nstats[shm->nstat_count++];
	bzero(ns, sizeof (newstats_t));

	ns->type = type;
	ns->sid = sid;
	ns->gid = gid;
	ns->tid = tid;
	ns->fid = fid;
	strlcpy(ns->name, name, sizeof (ns->name));

	return (ns);
}
