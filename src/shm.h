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

#ifndef _SHM_H
#define	_SHM_H

#include "logging.h"
#include "sync.h"
#include "workorder.h"


#define	NUM_STATES	10
#define	NUM_BARRIER	100	/* How many transactions per profile? */

#define LIST_APPEND_WITH_LOCK(b)	\
    pthread_mutex_lock(&shared->lock);	\
    llist_append(&shared->slist, (b));	\
    pthread_mutex_unlock(&shared->lock);


struct control_connection {
	protocol_t *p;
	int group;
	char host[MAXHOSTNAME];
};

struct uperf_shm {
	barrier_t begin_run;
	barrier_t end_run;
	barrier_t signal_barrier;
	int nobarrier;
	int init;
	pthread_mutex_t lock;
	pthread_mutexattr_t attr;
	barrier_t bar[NUM_BARRIER];

	int endian;
	int bitswap;
	int size;			/* Size of shared area */
	role_t role;
	int tx_no_slave_info;		/* Used by slave */
	int rx_no_slave_info;		/* No of slave_info_t rx by slave*/
	hrtime_t current_time;		/* Used by duration option in flowp */
	char host[MAXHOSTNAME];		/* name of the master*/
	uint64_t bytes_xfer;		/* Total bytes transferred so far */
	uint64_t txn_count;		/* Total bytes transferred so far */
	newstats_t *nstats;
	newstats_t agg_stat;
	int nstats_size;
	int nstat_count;
	workorder_t *workorder;		/* USED BY MASTER */
	group_t *worklist; 		/* USED BY SLAVE */

	/* control connections */
	/* int no_conn;
	struct control_connection cconnections[MAX_REMOTE_HOSTS];
	*/
	protocol_t *control;		/* Slave control connection */

	/* Error handling */
	uperf_log_t log;
	int global_error;
	uint32_t sstate1[NUM_STATES];
	uint32_t finished;
	int cleaned_up;

	/* callouts */
	hrtime_t callouts[MAXTHREADGROUPS];
	hrtime_t txn_begin;
	
	/* per thread structures */
	protocol_t **connection_list;
	int no_strands;
	strand_t strands[];
};

extern uperf_shm_t *global_shm;

uperf_shm_t *	shm_init(workorder_t *w);
void 		shm_fini(uperf_shm_t *);
strand_t * 	shm_get_strand(uperf_shm_t *shm, int id);
barrier_t * shm_get_barrier(uperf_shm_t *shm, int grp, int txn);
int shm_init_barriers_master(uperf_shm_t *shm, workorder_t *w);
void flag_error(char *reason);
int shm_init_barriers_slave(uperf_shm_t *shm, group_t *g);
int bump_global_xfer(uint64_t val);
int shm_callout_register(uperf_shm_t *, hrtime_t, int);
int shm_process_callouts(uperf_shm_t *);
void shm_update_strand_exit(uperf_shm_t *);
newstats_t * malloc_newstats(uperf_shm_t *, stats_type_t, int, int, int, int, char *);

#endif /* _SHM_H */
