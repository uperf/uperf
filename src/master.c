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
#endif	/* HAVE_CONFIG_H */
#ifdef HAVE_SYS_LWP_H
#include <sys/lwp.h>
#endif /* HAVE_SYS_LWP_H */
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif /* HAVE_POLL_H */

#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif /* HAVE_SYS_POLL_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "uperf.h"
#include "protocol.h"
#include "sync.h"
#include "logging.h"
#include "main.h"
#include "flowops.h"
#include "workorder.h"
#include "strand.h"
#include "shm.h"
#include "handshake.h"
#include "netstat.h"
#include "goodbye.h"
#include "print.h"
#include "signals.h"
#include "common.h"
#include "stats.h"
#include "print.h"

#define	UPERF_GOODBYE_TIMEOUT	15000	/* 15 seconds */
#define	MAX_POLL_SLAVES_TIMEOUT	1000	/* 1 second */

extern options_t options;

static protocol_t *slaves[MAXSLAVES];
static int no_slaves;

static int
say_goodbye(goodbye_stat_t *total, protocol_t *p, int timeout)
{
	goodbye_t g;
	char msg[GOODBYE_MESSAGE_LEN + MAXHOSTNAME + 4];

	if (recv_goodbye(&g, p, timeout) != UPERF_SUCCESS) {
		uperf_error("\nError saying goodbye with %s\n", p->host);
		return (1);
	}

	switch (g.msg_type) {
		case MESSAGE_INFO:
			(void) snprintf(msg, sizeof(msg),
			    "[%s] %s", p->host, g.message);
			uperf_info(msg);
			break;
		case MESSAGE_NONE:
			break;
		case MESSAGE_ERROR:
			(void) snprintf(msg, sizeof(msg),
			    "[%s] %s", p->host, g.message);
			uperf_log_msg(UPERF_LOG_ERROR, 0, msg);
			break;
		case MESSAGE_WARNING:
			(void) snprintf(msg, sizeof(msg),
			    "[%s] %s\n", p->host, g.message);
			uperf_log_msg(UPERF_LOG_WARN, 0, msg);
			break;
	}
	(void) print_goodbye_stat(p->host, &g.gstat);
	total->elapsed_time = MAX(g.gstat.elapsed_time, total->elapsed_time);
	total->error += g.gstat.error;
	total->bytes_xfer += g.gstat.bytes_xfer;
	total->count += g.gstat.count;

	return (0);
}

/*
 * Connect to all remote clients and exchange run statistics.
 * Any errors encountered by the slave are also received.
 */
static uint64_t
say_goodbyes_and_close(goodbye_stat_t *gtotal, int timeout)
{
	int i;

	print_goodbye_stat_header();
	for (i = 0; i < no_slaves; i++) {
		/*
		 * we continue on error, as there might be
		 * other slaves. We do not consider a
		 * failure to exchange goodbye's a reason to
		 * exit with error.
		 */
		(void) say_goodbye(gtotal, slaves[i], timeout);
		destroy_protocol(slaves[i]->type, slaves[i]);
		slaves[i] = NULL;
	}

	if (no_slaves > 1) {
		(void) print_goodbye_stat("Total", gtotal);
	}
	no_slaves = 0;

	return (0);
}

static int
group_assign_stat(uperf_shm_t *shm, group_t *g, uint32_t sid)
{
	txn_t *txn;
	flowop_t *fptr;

	g->stats = malloc_newstats(shm, NSTAT_GROUP, sid, GROUP_ID(g), -1,
	    -1, g->name);

	for (txn = g->tlist; txn; txn = txn->next) {
		txn->stats = malloc_newstats(shm, NSTAT_TXN, sid, GROUP_ID(g),
		    TXN_ID(txn), -1, txn->name);
		for (fptr = txn->flist; fptr; fptr = fptr->next) {
			fptr->stats = malloc_newstats(shm, NSTAT_FLOWOP, sid,
			    GROUP_ID(g), TXN_ID(txn), FLOWOP_ID(fptr),
			    fptr->name);
		}
	}

	return (0);
}

/*
 * Poll all the slaves for any commands. Slaves, currently only send
 * UPERF_STATS command
 * Return Values:
 * 0  : indicates timeout
 * +ve: indicates event is available. slave is updated to indicate
 *      one of the connections for which data is available.
 * -1 : indicates error
 */
static int
poll_slaves()
{
	int i;
	int error;
	struct pollfd pfd[MAXSLAVES];
	int no_pfds = no_slaves;
	uint64_t timeout;

	timeout = MIN(MAX_POLL_SLAVES_TIMEOUT, options.interval);
	for (i = 0; i < no_slaves; i++) {
		pfd[i].fd = slaves[i]->fd;
		pfd[i].events = POLLIN;
		pfd[i].revents = 0;
	}
	error = poll(pfd, no_pfds, timeout);
	if (error < 0) {
		perror("poll:");
	}

	return (error);
}

/* Send a command to all slaves */
static int
send_command_to_slaves(uperf_cmd cmd, int value)
{
	int ret = 0;
	int i;

	for (i = 0; i < no_slaves; i++) {
		ret = uperf_send_command(slaves[i], cmd, value);
		if (ret <= 0) {
			char msg[MAXHOSTNAME + 64];
			(void) snprintf(msg, sizeof (msg),
			    "Could not send command %d to %s:%d ",
			    value, slaves[i]->host, slaves[i]->port);
			uperf_log_msg(UPERF_LOG_WARN, ret, msg);
			break;
		}
	}
	return (ret);
}

static void
master_prepare_to_exit(uperf_shm_t *shm)
{
	static int cleaned_up = 0;
	if (cleaned_up > 1)
		abort();
	uperf_info("Master: Shutting down strands\n");
	strand_killall(shm);
	(void) send_command_to_slaves(UPERF_CMD_ABORT, 0);
	cleaned_up++;
}

static void
print_progress(uperf_shm_t *shm, newstats_t prev)
{
	if (ENABLED_STATS(options)) {
		newstats_t pns;
		update_aggr_stat(shm);
		(void) memcpy(&pns, AGG_STAT(shm), sizeof (pns));
		pns.start_time = prev.end_time;
		pns.size -= prev.size;
		pns.end_time = GETHRTIME();
		pns.count -= prev.count;
		(void) strlcpy(pns.name, prev.name, sizeof (pns.name));
		print_summary(&pns, 1);
	}
}

static int
master_poll(uperf_shm_t *shm)
{
	int no_txn;
	int error;
	int curr_txn = 0;
	barrier_t *curr_bar;
	double time_to_print;
	newstats_t prev_ns;

	bzero(&prev_ns, sizeof (prev_ns));

	no_txn = workorder_max_txn(shm->workorder);
	shm->current_time = GETHRTIME();
	time_to_print = shm->current_time;

	/*
	 * The main event loop. It runs roughly at options.interval
	 * frequency.
	 * We come out of the loop if any of the following is true
	 * 1. If slaves send anything (they should not in normal circumstances)
	 * 2. All Transactions except the last one are complete
	 * 3. Error
	 */
	while (curr_txn < no_txn) {
		curr_bar = &shm->bar[curr_txn];
		if (shm->global_error > 0) {
			break;
		}
		error = poll_slaves();
		if (error != 0) {	/* msg arrived */
			/* Read slave msg and process it */
			(void) printf("\n*** Slave aborted! ***\n");
			shm->global_error++;
			break;
		}
		shm->current_time = GETHRTIME();
		shm_process_callouts(shm);

		if (BARRIER_REACHED(curr_bar)) { /* goto Next Txn */
			if (ENABLED_STATS(options)) {
				if (curr_txn != 0) {
					print_progress(shm, prev_ns);
					(void) printf("\n");
				}
				update_aggr_stat(shm);
				(void) memcpy(&prev_ns, AGG_STAT(shm),
				    sizeof (prev_ns));
				prev_ns.end_time = GETHRTIME();
				(void) snprintf(prev_ns.name,
						sizeof (prev_ns.name),
						"Txn%d", curr_txn + 1);
			}
			/*
			 * Ask slaves to begin curr_txn Ok to ignore
			 * return value as number of transactions
			 * per group may not be the same
			 */
			(void) send_command_to_slaves(UPERF_CMD_NEXT_TXN,
			    curr_txn);
			/* release barrier so master can also begin curr_txn */
			shm->txn_begin = GETHRTIME();
			unlock_barrier(curr_bar);
			curr_txn++;
		}

		shm->current_time = GETHRTIME();
		if (ENABLED_STATS(options) &&
		    (time_to_print <= shm->current_time)) {
			print_progress(shm, prev_ns);
			time_to_print = shm->current_time
			    + options.interval * 1.0e+6;
		}
	}
	while (shm->global_error == 0 && shm->finished == 0) {
		shm_process_callouts(shm);
		print_progress(shm, prev_ns);
		(void) poll(NULL, 0, 100);
	}
	if (ENABLED_STATS(options)) {
		(void) printf("\n");
		uperf_line();
	}
	if (shm->global_error > 0) {
		master_prepare_to_exit(shm);
		return (1);
	}
	if (ENABLED_ERROR_STATS(options))  {
		return (send_command_to_slaves(UPERF_CMD_SEND_STATS, 0));
	} else {
		return (0);
	}
}

/* Create a control connection to a slave */
static int
new_control_connection(group_t *g, char *host)
{
	protocol_t *p;

	if (host == NULL || host[0] == '\0') {
		uperf_error("remotehost not specified. Check profile\n");
		return (UPERF_FAILURE);
	}
	/* First check if we already have one */
	p = g->control;
	while (p) {
		if (strncasecmp(host, p->host, MAXHOSTNAME) == 0) {
			uperf_info("Resuing control connection for %s\n", host);
			return (UPERF_SUCCESS);
		}
		p = p->next;
	}
	/* Not found, create a new one */
	p = create_protocol(options.control_proto, host, options.master_port,
			    MASTER);
	if (p != NULL) {
		/* Try connecting */
		if (p->connect(p, NULL) == 0) {
			p->next = g->control;
			if (g->control)
				g->control->prev = p;
			g->control = p;
			slaves[no_slaves++] = p;
			return (UPERF_SUCCESS);
		}
	}
	uperf_error("Error connecting to %s\n", host);
	return (UPERF_FAILURE);
}

static int
create_control_connections(workorder_t *w)
{
	int i;
	group_t *g;
	txn_t *t;
	flowop_t *f;

	for (i = 0; i < w->ngrp; i++) {
		g = &w->grp[i];
		for (t = g->tlist; t; t = t->next) {
			for (f = t->flist; f; f = f->next) {
				if (f->type == FLOWOP_CONNECT ||
				    f->type == FLOWOP_ACCEPT) {
					if (new_control_connection(
					    g,
					    f->options.remotehost) != 0)
						return (UPERF_FAILURE);
					g->protocols[f->options.protocol] = 1;
				}
			}
		}
	}
	return (UPERF_SUCCESS);
}

static uperf_shm_t *
master_init(workorder_t *w)
{
	uperf_shm_t *shm;

	if (protocol_init(NULL) == UPERF_FAILURE) {
		return (NULL);
	}
	if (master_setup_signal_handler() != UPERF_SUCCESS) {
		uperf_error("Error setting up signal handlers\n");
		return (NULL);
	}
	if ((shm = shm_init(w)) == NULL) {
		return (NULL);
	}
	if (create_control_connections(w) != UPERF_SUCCESS) {
		shm_fini(shm);
		return (NULL);
	}
	shm->endian = UPERF_ENDIAN_VALUE;
	shm->global_error = 0;
	shm->role = MASTER;
	shm->workorder = w;

	if (strand_init_all(shm, w) != UPERF_SUCCESS) {
		uperf_error("error in strand_init_all\n");
		shm_fini(shm);
		return (NULL);
	}

	/* Initialize barriers */
	if (shm_init_barriers_master(shm, shm->workorder) != 0) {
		shm_fini(shm);
		return (NULL);
	}

	return (shm);
}

static int
spawn_strands_group(uperf_shm_t *shm, group_t *gp, int id)
{
	int j;
	int rc;
	strand_t *s;

	for (j = 0; j < gp->nthreads; j++) {
		s = shm_get_strand(shm, id + j);
		s->worklist = group_clone(gp);
		s->shmptr = shm;
		s->role = MASTER;
		(void) group_assign_stat(shm, s->worklist, id + j);

		if (STRAND_IS_PROCESS(gp)) {
			s->strand_flag |= STRAND_TYPE_PROCESS;
			rc = fork();
			if (rc == 0) {
				(void) strand_run(s);
				exit(0);
			} else if (rc > 0) {
				s->pid = rc;
			} else {
				perror("fork");
				uperf_error("fork failed\n");
				return (1);
			}
		} else {
			s->strand_flag |= STRAND_TYPE_THREAD;
			rc = pthread_create(&(s->tid), NULL,
					&strand_run,
					(void *) s);
			if (rc) {
				perror("pthread_create");
				uperf_error("pthread_create failed\n");
				return (1);
			}
		}
	}
	return (0);
}

/*
 * Function: init_master Description: This is where the master gets
 * initialized. We need to create the requested number of threads, and
 * initialize the barrier.
 *
 * Each thread contacts the slave, and "export" the parameters structure Note
 * that since we might want to talk to multiple slaves, we loop through
 * the all the flowops and create control connections for each of those
 * hosts.
 *
 * When we export the parameter structure, we need to ensure that we blank
 * out operations that are not requested of that slave. For ex if master
 * connects to 2 slaves, each slave needs to only 1 accpect.
 */

int
master(workorder_t *w)
{
	int rc;
	int i;
	int thr_count;
	int nthr, nproc;
	int id;
	int goodbye_timeout = UPERF_GOODBYE_TIMEOUT;
	hrtime_t start, stop;
	uperf_shm_t *shm;
	goodbye_stat_t gtotal;
	int error;

	if ((shm = master_init(w)) == NULL)
		exit(1);

	/*
	 * We need to get the total number of threads to arm the
	 * start, and end barriers.
	 */
	nproc = workorder_num_strands_bytype(w, STRAND_TYPE_PROCESS);
	nthr = workorder_num_strands_bytype(w, STRAND_TYPE_THREAD);
	thr_count = nproc + nthr;

	if (handshake(shm, w) != UPERF_SUCCESS) {
		uperf_info("Error in handshake\n");
		shm_fini(shm);
		exit(1);
	}

	if (nthr == 0 && nproc != 0) {
		(void) printf("Starting %d processes running profile:%s ... ",
		    thr_count, w->name);
	} else if (nproc == 0 && nthr != 0) {
		(void) printf("Starting %d threads running profile:%s ... ",
		    thr_count, w->name);
	} else {
	(void) printf(
		"Starting %d threads, %d processes running profile:%s ... ",
		nthr, nproc, w->name);
	}
	(void) fflush(stdout);

	start = GETHRTIME();
	/* Traverse through the worklist and create threads */
	id = 0;
	for (i = 0; i < w->ngrp; i++) {
		w->grp[i].groupid = i;
		if (shm->global_error > 0)
			break;
		if (spawn_strands_group(shm, &w->grp[i], id) != 0)
			shm->global_error++;
		id += w->grp[i].nthreads;
	}

	if (shm->global_error > 0) {
		master_prepare_to_exit(shm);
		shm_fini(shm);
		return (UPERF_FAILURE);
	}
	stop = GETHRTIME();
	(void) printf(" %5.2f seconds\n", (stop-start)/1.0e+9);
	(void) fflush(stdout);

	/* Handshake end */
	if (handshake_end_master(shm, w) != UPERF_SUCCESS) {
		return (UPERF_FAILURE);
	}

#ifdef ENABLE_NETSTAT
	if (ENABLED_PACKET_STATS(options))
		netstat_snap(SNAP_BEGIN);
#endif /* ENABLE_NETSTAT */

	/*
	 * The main Loop.
	 * if either global_error is not 0 or master_poll returns 1
	 * let wait_for_strands know that.
	 */
	newstat_begin(0, AGG_STAT(shm), 0, 0);
	error =  master_poll(shm);
	if (error == 0 && shm->global_error != 0)
		error = shm->global_error;

	(void) wait_for_strands(shm, error);
	newstat_end(0, AGG_STAT(shm), 0, 0);

	shm->current_time = GETHRTIME();

#ifdef ENABLE_NETSTAT
	if (ENABLED_PACKET_STATS(options))
		netstat_snap(SNAP_END);
#endif /* ENABLE_NETSTAT */
	if (ENABLED_STATS(options)) {
		print_summary(AGG_STAT(shm), 0);
	}

	if (shm->global_error > 0) {
		/* decrease timeout coz no point in waiting */
		goodbye_timeout = 1000;
	}

	if (ENABLED_GROUP_STATS(options))
		print_group_details(shm);
	if (ENABLED_THREAD_STATS(options))
		print_strand_details(shm);
	if (ENABLED_TXN_STATS(options))
		print_txn_averages(shm);
	if (ENABLED_FLOWOP_STATS(options))
		print_flowop_averages(shm);
#ifdef ENABLE_NETSTAT
	if (ENABLED_PACKET_STATS(options))
		print_netstat();
#endif /* ENABLE_NETSTAT */
	if (ENABLED_ERROR_STATS(options)) {
		goodbye_stat_t local;

		(void) memset(&gtotal, 0, sizeof (goodbye_stat_t));
		if ((rc = say_goodbyes_and_close(&gtotal, goodbye_timeout))
		    == 0) {
			update_aggr_stat(shm);
			local.elapsed_time = (AGG_STAT(shm))->end_time
			    - (AGG_STAT(shm))->start_time;
			local.error = 0;
			local.bytes_xfer = (AGG_STAT(shm))->size;
			local.count = (AGG_STAT(shm))->count;
			print_goodbye_stat("master", &local);
			print_difference(local, gtotal);
		}
	}
	uperf_log_flush();

	if (ENABLED_HISTORY_STATS(options)) {
		(void) fclose(options.history_fd);
	}
	/* Cleanup */
	if (shm->global_error != 0) {
		(void) printf("\nWARNING: %d Errors detected during run\n",shm->global_error);
		shm_fini(shm);
		exit(1);
	}
	shm_fini(shm);

	return (rc);
}
