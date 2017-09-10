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
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#ifdef USE_PROC
#include <procfs.h>
#include <sys/procfs.h>
#endif /* USE_PROC */
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h>
#include "uperf.h"
#include "main.h"
#include "workorder.h"
#include "stats.h"
#include "strand.h"

#ifdef HAVE_MACH_ABSOLUTE_TIME
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif /* HAVE_MACH_ABSOLUTE_TIME */

#ifdef USE_CPC
#include "hwcounter.h"
#endif /* USE_CPC */
#include "shm.h"

extern options_t options;
extern uperf_shm_t *global_shm;

#ifdef HAVE_GETHRTIME
#define	GETHRTIME	gethrtime
#elif HAVE_CLOCK_GETTIME
#define	SEC2NANOSEC	1000000000LL
uint64_t
GETHRTIME()
{
	struct timespec now;
	uint64_t ret = 0;
	clock_gettime(CLOCK_REALTIME, &now);

	ret = now.tv_sec * SEC2NANOSEC + now.tv_nsec;

	return (ret);
}
#elif HAVE_MACH_ABSOLUTE_TIME
uint64_t
GETHRTIME()
{
	static mach_timebase_info_data_t sTimebaseInfo;

	if ( sTimebaseInfo.denom == 0 ) {
		(void) mach_timebase_info(&sTimebaseInfo);
	}
	return mach_absolute_time() * sTimebaseInfo.numer/sTimebaseInfo.denom;
}
#else
#error "Could not find gethrtime nor clock_gettime"
#endif /* HAVE_GETHRTIME */

#ifdef HAVE_GETHRVTIME
#define	GETHRVTIME gethrvtime
#else
uint64_t
GETHRVTIME()
{
	return (0);
}
#endif /* HAVE_GETHRVTIME */

/* ARGSUSED */
int
newstat_begin(strand_t *s, newstats_t *ns, uint64_t size, uint64_t count)
{
	if (ns == NULL)
		return (0);
	if (ns->start_time == 0)
		ns->start_time = GETHRTIME();
	if (ns->min == 0)
		ns->min = ULONG_MAX;
	ns->time_used_start = GETHRTIME();
	if (ENABLED_UTILIZATION_STATS(options))
		ns->cpu_time_start = GETHRVTIME();
#ifdef USE_CPC
	if (s && ENABLED_CPUCOUNTER_STATS(options))
		hwcounter_snap(&s->hw, SNAP_BEGIN);
#endif
	return (0);
}

int
newstat_end(strand_t *s, newstats_t *ns, uint64_t size, uint64_t count)
{
	uint64_t delta;

	if (ns == NULL)
		return (0);

	ns->end_time = GETHRTIME();
	if (ENABLED_UTILIZATION_STATS(options))
		ns->cpu_time_start = GETHRVTIME();
	ns->size += size;
	ns->count += count;
	delta = ns->end_time - ns->time_used_start;
	ns->time_used += delta;
	ns->max = MAX(ns->max, delta);
	ns->min = MIN(ns->min, delta);
#ifdef USE_CPC
	if (s && ENABLED_CPUCOUNTER_STATS(options)) {
		hwcounter_snap(&s->hw, SNAP_END);
		ns->pic0 += hwcounter_get(&s->hw, 0);
		ns->pic1 += hwcounter_get(&s->hw, 1);
	}
#endif
	return (0);
}

int
stats_update(int type, strand_t *s, newstats_t *stats, uint64_t size,
    uint64_t count)
{
	if (DISABLED_STATS(options))
		return (0);

	switch (type) {

	case FLOWOP_BEGIN:
		if (ENABLED_FLOWOP_STATS(options) ||
		    ENABLED_GROUP_STATS(options) ||
		    ENABLED_HISTORY_STATS(options)) {
			return (newstat_begin(s, stats, 0, 0));
		}
		return (0);
	case FLOWOP_END:
		/* We update the strand stats instead of having a global one */
		STRAND_STAT(s)->size += size*count;	/* Thread safe */
		STRAND_STAT(s)->count += count;		/* Thread safe */

		if (ENABLED_FLOWOP_STATS(options) ||
		    ENABLED_GROUP_STATS(options) ||
		    ENABLED_HISTORY_STATS(options)) {
			int err = newstat_end(s, stats, size, count);
			if (ENABLED_HISTORY_STATS(options)) {
				history_record(s, NSTAT_FLOWOP, stats->end_time,
				    stats->end_time - stats->time_used_start);
			}
			return (err);
		}
		return (0);
	case TXN_BEGIN:
		if (ENABLED_TXN_STATS(options))
			return (newstat_begin(s, stats, 0, 0));
		return (0);
	case GROUP_BEGIN:
		if (ENABLED_GROUP_STATS(options))
			return (newstat_begin(s, stats, 0, 0));
		return (0);
	case TXN_END:
		if (ENABLED_TXN_STATS(options))
			return (newstat_end(s, stats, 0, count));
		return (0);
	case GROUP_END:
		if (ENABLED_GROUP_STATS(options))
			return (newstat_end(s, stats, 0, count));
		return (0);
	}

	return (0);
}

/* s1 = s1 + s2 */
void
add_stats(newstats_t *s1, newstats_t *s2)
{
	s1->count += s2->count;
	s1->time_used += s2->time_used;
	s1->cpu_time += s2->cpu_time;
	s1->size += s2->size;
	s1->pic0 += s2->pic0;
	s1->pic1 += s2->pic1;

	s1->start_time = MIN(s1->start_time, s2->start_time);
	s1->end_time = MAX(s1->end_time, s2->end_time);
	s1->max = MAX(s1->max, s2->max);
	s1->min = MIN(s1->min, s2->min);
}

void
update_aggr_stat(uperf_shm_t *shm)
{
	int i;

	AGG_STAT(shm)->size = 0;
	AGG_STAT(shm)->count = 0;
	for (i = 0; i < shm->no_strands; i++) {
		newstats_t *ns = STRAND_STAT(shm_get_strand(shm, i));
		AGG_STAT(shm)->size += ns->size;
		AGG_STAT(shm)->count += ns->count;
	}
}

void
history_init(strand_t *s)
{
	s->history = (history_t *)calloc(HISTORY_PER_STRAND,
	    sizeof (history_t));
	s->hsize = 0;
}

void
flush_history(strand_t *s)
{
	int i;

	if (s == NULL) {
		printf("history is NULL\n");
		return;
	}
	for (i = 0; i < s->hsize; i++) {
		history_t *h = &(s->history[i]);
		fprintf(options.history_fd, "%6d %6lu %2d %15"PRIu64" %15"PRIu64"\n",
		    (int) s->pid, (unsigned long)s->tid, h->type, h->etime, h->delta);
	}
	s->hsize = 0;
}

void
history_record(strand_t *s, uint32_t type, uint64_t etime,
    uint64_t delta)
{
	history_t h = {type, etime, delta};

	if (s->hsize >= HISTORY_PER_STRAND) {
		flush_history(s);
	}
	s->history[s->hsize++] = h;
}
