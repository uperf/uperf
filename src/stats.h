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

#ifndef _STATS_H
#define	_STATS_H

#define	AGG_STAT(A)	(&((A)->agg_stat))
#define	STRAND_STAT(S)	(&((S)->nstats))
#define	GROUP_STAT(S)	(S)->stats
#define	TXN_STAT(S)	(S)->stats
#define	FLOWOP_STAT(S)	(S)->stats

#define	FLOWOP_BEGIN	1
#define	FLOWOP_END	2
#define	TXN_BEGIN	3
#define	TXN_END		4
#define	GROUP_BEGIN	5
#define	GROUP_END	6

#define	AGG_STAT_NAME	"Total"
#define	UPERF_NAME_LEN	32

typedef enum {
	NSTAT_FLOWOP,
	NSTAT_TXN,
	NSTAT_GROUP,
	NSTAT_APP,
	NSTAT_STRAND
} stats_type_t;

typedef struct _newstats_t {
	uint64_t start_time;
	uint64_t end_time;
	uint64_t time_used_start;
	uint64_t cpu_time_start;
	uint64_t max;
	uint64_t min;
	uint64_t size;
	uint64_t count;
	uint64_t time_used;
	uint64_t cpu_time;
	uint64_t pic0;
	uint64_t pic1;
	stats_type_t type;	/* Type (FLOWOP, TXN, GROUP, STRAND, OVERALL) */
	uint32_t sid;	/* Strand id */
	uint32_t gid;	/* Group id */
	uint32_t tid;	/* Txn id */
	uint32_t fid;	/* Flowop id */
	char name[UPERF_NAME_LEN];
}newstats_t;

#define STATS_RECORD_FLOWOP(A, S, F, B, C)	\
    if (ENABLED_STATS(options)) \
	 stats_update((A), (S), (F), (B), (C));
int stats_update(int type, strand_t *s, newstats_t *stat, uint64_t, uint64_t);
int newstat_begin(strand_t *, newstats_t *, uint64_t, uint64_t);
int newstat_end(strand_t *, newstats_t *, uint64_t, uint64_t);
void add_stats(newstats_t *s1, newstats_t *s2);
void update_aggr_stat(uperf_shm_t *shm);


#define	HISTORY_PER_STRAND	8192
typedef struct _hist {
	uint32_t type;
	uint64_t etime;
	uint64_t delta;
}history_t;

void history_init(strand_t *s);
void flush_history(strand_t *s);
void history_record(strand_t *, uint32_t, uint64_t, uint64_t);

#endif /* _STATS_H */
