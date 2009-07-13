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

#ifndef _STRAND_H
#define	_STRAND_H

#include "uperf.h"
#ifdef USE_CPC
#include "hwcounter.h"
#endif /* USE_CPC */

typedef struct slave_info {
	char 			host[MAXHOSTNAME];
	int 			port[NUM_PROTOCOLS];
}slave_info_t;

typedef struct slave_info_list {
	slave_info_t		slave;
	struct slave_info_list	*next;
}slave_info_list_t;

typedef enum {
	STRAND_STATE_AT_BARRIER,
	STRAND_STATE_EXECUTING,
	STRAND_STATE_EXIT,
} strand_state_t;
#define	STRAND_AT_BARRIER(s)	((s)->strand_state == STRAND_STATE_AT_BARRIER)
#define	STRAND_EXECUTING(s)	((s)->strand_state == STRAND_STATE_EXECUTING)
#define	STRAND_EXIT(s)	((s)->strand_state == STRAND_STATE_EXIT)

#define SIGNALLED(A)	((A)->signalled == 1)
#define CLEAR_SIGNAL(A)	(A)->signalled = 0


#define	STRAND_CONNECTION_CACHE_SIZE	8

struct uperf_strand {
	/*
	 * This is used to keep a list of all opened
	 * connections by this strand
	 */
	int 		no_connections1;
	protocol_t 	**connections1;
	protocol_t	*listen_conn[NUM_PROTOCOLS];
	protocol_t	*ccache[STRAND_CONNECTION_CACHE_SIZE];
	int		ccache_size;
	protocol_t	*cpool;

	/*
	 * List of slaves this strand connects to. This is used to keep 
	 * track of the port that this strand connects to
	 */
	slave_info_list_t 	*slave_list;

	pid_t		pid;
	pthread_t	tid;	/* actual thread id */
	int 		role;
	volatile uint32_t	strand_flag;
	volatile uint32_t	signalled;
	volatile strand_state_t	strand_state;
	group_t		*worklist;
	char 		*buffer;
#ifdef USE_CPC
	hwcounter_t 	hw;
#endif
	uint64_t	errors;	/* Error execute count */
	uint64_t	datasz;
	newstats_t 	nstats;
	history_t	*history;
	uint64_t	hsize;
	uperf_shm_t	*shmptr;
};

int strand_add_slave(strand_t *, slave_info_t *);
int strand_init_all(uperf_shm_t *, workorder_t *);
int strand_init_group(uperf_shm_t *, group_t *, int);
protocol_t *strand_get_connection(strand_t *, int);
int strand_get_port(strand_t *, char *, int);
int strand_add_connection(strand_t *, protocol_t *);
int strand_delete_connection(strand_t *, int);
int signal_all_strands(uperf_shm_t *, int, int);
int strand_killall(uperf_shm_t *);
void wait_for_strands(uperf_shm_t *, int);
int signal_strand(strand_t *s, int signal);
void strand_fini(strand_t *s);
void * strand_run(void *sp);
#endif /* _STRAND_H */
