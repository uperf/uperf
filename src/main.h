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

#ifndef	 __MAIN_H
#define	 __MAIN_H
#include <limits.h>			/* PATH_MAX */
#include <stdio.h>			/* PATH_MAX */
#include <sched.h>
#include "protocol.h"

#define	FLOWOP_STATS		(1<<0)
#define	TXN_STATS		(1<<1)
#define	CPUCOUNTER_STATS	(1<<2)
#define	PACKET_STATS		(1<<3)
#define	THREAD_STATS		(1<<4)
#define	ERROR_STATS		(1<<5)
#define	HISTORY_STATS		(1<<8)
#define	GROUP_STATS		(1<<9)
#define	UTILIZATION_STATS	(1<<11)
#define	NO_STATS		(1<<12)
#define RAW_STATS		(1<<13)

#define	ENABLED_FLOWOP_STATS(a)		((a).copt & FLOWOP_STATS)
#define	ENABLED_TXN_STATS(a)		((a).copt & TXN_STATS)
#define	ENABLED_CPUCOUNTER_STATS(a)	((a).copt & CPUCOUNTER_STATS)
#define	ENABLED_PACKET_STATS(a)		((a).copt & PACKET_STATS)
#define	ENABLED_THREAD_STATS(a)		((a).copt & THREAD_STATS)
#define	ENABLED_ERROR_STATS(a)		((a).copt & ERROR_STATS)
#define	ENABLED_HISTORY_STATS(a)	((a).copt & HISTORY_STATS)
#define	ENABLED_GROUP_STATS(a)		((a).copt & GROUP_STATS)
#define	ENABLED_UTILIZATION_STATS(a)	((a).copt & UTILIZATION_STATS)
#define	DISABLED_STATS(a)		((a).copt & NO_STATS)
#define	ENABLED_STATS(a)		(!DISABLED_STATS(a))
#define ENABLED_RAW_STATS(a)		((a).copt & RAW_STATS)

#define	UPERF_MASTER		(1<<0)
#define	UPERF_SLAVE		(1<<1)

#define	IS_MASTER(a)		((a).run_choice & UPERF_MASTER)
#define	IS_SLAVE(a)		((a).run_choice & UPERF_SLAVE)

#define	MAX_ZC_QUEUES	32

/* options structure - has basic program options */
typedef struct options {
	int	master_port;
	int	run_choice;	/* master or slave */
	char	app_profile_name[PATH_MAX];
	int	bitorbyte;
	int	xanadu_print;
	FILE	*history_fd;
	char	xfile[PATH_MAX];
	char	*ev1;
	char	*ev2;
	uint32_t copt;	/* Collect options */
	uint64_t interval;	/* collect stats every interval msecs */
	proto_type_t control_proto;
	int zc_ifindex;
	int zc_queue_index[MAX_ZC_QUEUES];
	int zc_cpu[MAX_ZC_QUEUES];
	unsigned int has_main_thread, main_thread;	/* cpu main thread */
	unsigned int has_worker_thread, worker_thread;	/* cpu worker thread */	
}options_t;

static int
move_to_core(int core_i)
{
        cpu_set_t cpus;

        CPU_ZERO(&cpus);
        CPU_SET(core_i, &cpus);
        return sched_setaffinity(0, sizeof(cpus), &cpus);
}

#endif /* __MAIN_H */
