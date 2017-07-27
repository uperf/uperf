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

#ifndef _WORKORDER_H
#define _WORKORDER_H

#define	NAMELEN	128

#include "uperf.h"
#include "flowops.h"
#include "protocol.h"
#include "stats.h"

#define	O_TCP_NODELAY		(1 << 1)
#define	O_CANFAIL		(1 << 2)
#define	O_NONBLOCKING		(1 << 3)
#define	O_THINK_IDLE		(1 << 4)
#define	O_THINK_BUSY		(1 << 5)
#define	O_SIZE_RAND		(1 << 6)
#define	O_SCTP_UNORDERED	(1 << 7)
#define	O_SCTP_NODELAY		(1 << 8)

#define	FO_TCP_NODELAY(fo)	((fo)->flag & O_TCP_NODELAY)
#define	FO_CANFAIL(fo)		((fo)->flag & O_CANFAIL)
#define	FO_NONBLOCKING(fo)	((fo)->flag & O_NONBLOCKING)
#define	FO_THINK_BUSY(fo)	((fo)->flag & O_THINK_BUSY)
#define	FO_THINK_IDLE(fo)	((fo)->flag & O_THINK_IDLE)
#define	FO_RANDOM_SIZE(fo)	((fo)->flag & O_SIZE_RAND)
#define	FO_SCTP_UNORDERED(fo)	((fo)->flag & O_SCTP_UNORDERED)
#define	FO_SCTP_NODELAY(fo)	((fo)->flag & O_SCTP_NODELAY)

#define	CLEAR_FO_NONBLOCKING(f)	((f->flag &= ~O_NONBLOCKING))

struct flowop_options {
	uint32_t	size;		/* In bytes */
	uint32_t	rand_sz_min;
	uint32_t	rand_sz_max;
	uint32_t	rsize;	/* Size to be used by slave for IO */
	uint32_t	protocol;	/* Which protocol to use */
	uint32_t	port;
	uint32_t	flag;
	uint32_t	nfiles;
	uint32_t	encaps_port;	/* Port used for UDP encapsulation */
	uint64_t	duration;	/* In nanoseconds */
	uint64_t	wndsz;		/* SCTP/TCP/UDP Window size */
	uint64_t	count;		/* Flowop execute Count */
	uint64_t	poll_timeout;	/* In nanoseconds */
	uint32_t	sctp_rto_min;		/* Minimum SCTP RTO */
	uint32_t	sctp_rto_max;		/* Maximum SCTP RTO */
	uint32_t	sctp_rto_initial;	/* Initial SCTP RTO */
	uint32_t	sctp_sack_delay;	/* SCTP SACK delay in ms */
	uint32_t	sctp_sack_frequency;	/* SCTP SACK frequency */
	uint32_t	sctp_max_burst_size;	/* SCTP Maximum burst size in MTU */
	uint32_t	sctp_max_fragment_size;	/* SCTP Maximum fragment size in bytes */
	uint32_t	sctp_hb_interval;	/* SCTP HEARTBEAT interval in ms */
	uint32_t	sctp_path_mtu;		/* SCTP path MTU in bytes */
	uint16_t	sctp_out_streams;	/* Number of outgoing SCTP streams */
	uint16_t	sctp_in_streams;	/* Number of incoming SCTP streams */
	uint16_t	sctp_stream_id;		/* SCTP stream identifier (SID) */
	uint16_t	sctp_padding;		/* To be 32-bit aligned */
	uint32_t	sctp_pr_value;		/* Value for PR-SCTP */
	char		sctp_pr_policy[8];	/* Method of PR-SCTP */
	char		cc[32];			/* CC algorithm to be used */
	char		stack[32];		/* TCP stack to be used */
	char		dir[PATHMAX];
	char		remotehost[MAXHOSTNAME];
	char		localhost[MAXHOSTNAME]; /* Localhost info as may be required in some protocols such as RDS */
	char		engine[20];	/*bundled engine, openssl is default*/
	char		cipher[50];	/*cipher suite*/
	char		method[5];	/*ssl or tls, ssl is default*/
	char		padding[5];
};

typedef int (*execute_func)(strand_t *, flowop_t *);

struct flowop {
	flowop_options_t options;
	flowop_type_t type;
	uint32_t p_id;
	uint32_t statid1;
	uint32_t errors;
	struct flowop *next;
	execute_func execute;
	newstats_t *stats;
	protocol_t *connection;
	uint32_t id;
	char name[UPERF_NAME_LEN];
};

#define SIZEOF_FLOWOP_T	offsetof(flowop_t, next)
#define	FLOWOP_ID(a) (a)->id

struct transaction {
	uint32_t nflowop;
	uint32_t txnid;
	uint32_t statid1;
	uint32_t unused;
	uint32_t rate_count;
	uint32_t dummy;
	uint64_t iter;
	uint64_t duration;	/* In milliseconds */
	char rate_str[NAMELEN];
	struct transaction *next;
	flowop_t *flist;
	int (*execute)(strand_t *, struct transaction *);
	newstats_t *stats;
	char name[UPERF_NAME_LEN];
};

#define SIZEOF_TXN_T	offsetof(txn_t, next)
#define	TXN_ID(a) (a)->txnid

struct thg {
	uint32_t endian;
	uint32_t strand_flag;
	uint32_t nthreads;
	uint32_t ntxn;
	uint32_t max_async;
	uint32_t groupid;
	txn_t *tlist;			/* List of transactions */
	protocol_t *control;		/* slave connections */
	int protocols[NUM_PROTOCOLS];	/* Protocols used */
	newstats_t *stats;
	uint32_t statid1;
	char name[UPERF_NAME_LEN];
};

#define	SIZEOF_GROUP_T	offsetof(group_t, tlist)
#define	GROUP_ID(a) (a)->groupid

typedef struct app_profile {
	char name[NAMELEN];
	int ngrp;
	group_t grp[MAXTHREADGROUPS];
}workorder_t;

int workorder_num_strands(workorder_t *);
int workorder_num_stats(workorder_t *);
int workorder_num_strands_bytype(workorder_t *, uint32_t);
int group_max_open_connections(group_t *);
int workorder_num_connections(workorder_t *);
int group_max_dto_size(group_t *);
void group_free(group_t *);
group_t* group_clone(group_t *);
int group_opposite(group_t *);
int group_bitswap(group_t *);
int workorder_max_txn(workorder_t *);

#endif /* _WORKORDER_H */
