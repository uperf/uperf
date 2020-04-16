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

/*
 * The slave The slave listens on a well known port. When it receives a
 * txn_group request, it accepts it, and creates a new thread to handle
 * that txn_group
 */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <pthread.h>
#include <sys/wait.h>

#ifdef UPERF_LINUX
#include <sys/poll.h>
#endif /* UPERF_LINUX */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

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
#include "goodbye.h"
#include "common.h"
#include "generic.h"
#include "stats.h"

extern options_t options;
static uperf_log_t log;
static int reap_children = 0;

static void slave_master_goodbye(uperf_shm_t *shm, protocol_t *control);

static int
slave_spawn_strands(uperf_shm_t *shm, protocol_t *control)
{
	int i;

	for (i = 0; i < shm->worklist->nthreads; i++) {
		int status;
		strand_t *s = shm_get_strand(shm, i);
		s->role = SLAVE;
		s->worklist = group_clone(shm->worklist);
		s->shmptr = shm;
		status = pthread_create(&s->tid, NULL, &strand_run, s);
		if (status != 0) {
			perror("pthread_create:");
			slave_handshake_p2_failure("Error creating threads",
			    control, 0);
			uperf_fatal("Quitting\n");
		}

	}
	shm->no_strands = shm->worklist->nthreads;

	return (0);
}

/*
 * Wait (for a max of 5s) for threads to reach a barrier, and unlock it
 */
static int
wait_unlock_barrier(uperf_shm_t *shm, int txn)
{
	int wait_time = 5;
	char msg[128];
	barrier_t *bar;

	if (txn > shm->worklist->ntxn)
		return (0);

	bar = &shm->bar[txn];
	while (BARRIER_NOTREACHED(bar)) {
		uperf_log_flush();
		if (shm->global_error > 0)
			return (-1);

		uperf_info("%d threads not at barrier %d sending SIGUSR2\n",
		    barrier_notreached(bar), txn);
		if (signal_all_strands(shm, -1, SIGUSR2) != 0) {
			uperf_log_msg(UPERF_LOG_ERROR, 0,
			    "Slave: Error sending signal to strands");
			return (-1);
		}
		(void) sleep(1);
		if (shm_process_callouts(shm) < 0) {
			uperf_log_msg(UPERF_LOG_ERROR, 0,
			    "Error processing callouts");
			    return (-1);
		}
		if (wait_time-- <= 0) {
			(void) snprintf(msg, sizeof (msg),
			    "Error waiting for strands at barrier %d", txn);
			uperf_log_msg(UPERF_LOG_ERROR, 0, msg);
			return (-1);
		}
	}
	uperf_info("%d: unlocking barrier %d\n", getpid(), txn);

	unlock_barrier(bar);

	return (0);
}

/*
 * State 1:
 * 	if (message from master)
 *	case SET_TXN:
 *		set txn for all threads
 *		GOTO to state1
 *	case finish -- NOT IMPLEMENTED
 * 		goto State2
 *	case EXIT: -- NOT IMPLEMENTED
 *		kill threads
 * 		goto state3
 */
static int
slave_master_poll(uperf_shm_t *shm, protocol_t *control)
{
	uperf_command_t uc;

	for (;;) {
		shm->current_time = GETHRTIME();
		if (shm->global_error > 0) {
			return (-1);
		}
		if (generic_poll(control->fd, 1000, POLLIN) > 0) {
			if ((uperf_get_command(control, &uc, shm->bitswap)
			    != 0)) {
				uperf_error("Error in get command\n");
				return (-1);
			}
			if (uc.command == UPERF_CMD_NEXT_TXN) {
				/* Unlock this barrier */
				shm->txn_begin = GETHRTIME();
				if (wait_unlock_barrier(shm, uc.value) != 0) {
					return (-1);
				}
			} else if (uc.command == UPERF_CMD_ABORT) {
				uperf_info("Got abort command\n");
				return (-1);
			} else if (uc.command == UPERF_CMD_SEND_STATS) {
				break;
			} else {
				uperf_error("Unknown command %d\n", uc.command);
				return (-1);
			}
		}
		shm_process_callouts(shm);
		if (shm->finished == shm->no_strands)
			uperf_info("Strands finished, but waiting for master \
cmd\n");
	}
	shm->current_time = -1;

	return (0);
}

static void
slave_master_goodbye(uperf_shm_t *shm, protocol_t *control)
{
	goodbye_t goodbye;

	(void) memset(&goodbye, 0, sizeof (goodbye_t));
	/* Gather stats */
	update_aggr_stat(shm);
	goodbye.gstat.elapsed_time = (AGG_STAT(shm))->end_time
	    - (AGG_STAT(shm))->start_time;
	goodbye.gstat.error = 0;
	goodbye.gstat.bytes_xfer = (AGG_STAT(shm))->size;
	goodbye.gstat.count = (AGG_STAT(shm))->count;
	/* Send goodbye */
	if (uperf_log_num_msgs() > 0) {
		goodbye.msg_type = MESSAGE_ERROR;
		uperf_log_flush_to_string(goodbye.message,
			GOODBYE_MESSAGE_LEN);
		uperf_info("Goodbye **  %s ***\n", goodbye.message);
	} else {
		goodbye.msg_type = MESSAGE_INFO;
		(void) strlcpy(goodbye.message, "Success", GOODBYE_MESSAGE_LEN);
	}
	if (shm->bitswap)
		bitswap_goodbye_t(&goodbye);
	send_goodbye(&goodbye, control);
}

static uperf_shm_t *
slave_init(protocol_t *p)
{
	int shm_size;
	int strand_size;
	int connlist_size;
	uperf_shm_t *shm;
	uperf_shm_t *shm_tmp;

	shm_tmp = calloc(1, sizeof (uperf_shm_t));
	if (slave_handshake(shm_tmp, p) != UPERF_SUCCESS) {
		free(shm_tmp);
		exit(1);
	}

	/* recreate the shm structure with storage for the strands */
	strand_size = shm_tmp->worklist->nthreads * sizeof (strand_t);
	connlist_size = shm_tmp->worklist->nthreads
	    * group_max_open_connections(shm_tmp->worklist)
	    * sizeof (protocol_t *);
	shm_size = sizeof (uperf_shm_t) + strand_size + connlist_size;

	if ((shm = calloc(1, shm_size)) == NULL) {
		slave_handshake_p2_failure("Out of Memory", p, 0);
		free(shm_tmp);
		return (NULL);
	}
	(void) memcpy(shm, shm_tmp, sizeof (uperf_shm_t));
	shm->connection_list = (protocol_t **)((char *)shm +
	    sizeof (uperf_shm_t) + strand_size);
	shm->endian = UPERF_ENDIAN_VALUE;
	shm->control = p;
	shm->role = SLAVE;
	global_shm = shm;

	free(shm_tmp);

	return (shm);
}
/*
 * Slave_master is a state machine
 * State 0: Init
 * 	Handshake with master
 * 	Create ports
 *	create threads
 *	GOTO state1
 * State 1:
 *	See slave_master_poll()
 * State2:
 *	wait for threads to finish
 * state3:
 *	join all threads
 *	goodbye with master
 */
static int
slave_master(protocol_t *p)
{
	slave_info_t *sl = NULL;
	uperf_shm_t *shm;
	group_t *g;
	int error = 0;

	if ((shm = slave_init(p)) == NULL) {
		uperf_error("Error initializing slave\n");
		exit(1);
	}
	g = shm->worklist;

	if (strand_init_group(shm, g, 0) != UPERF_SUCCESS) {
		uperf_error("error in strand_init_all\n");
		slave_handshake_p2_failure("error in strand_init_all",
		    p, 0);
		free(shm);
		return (UPERF_FAILURE);
	}

	if (shm->rx_no_slave_info > 0) {
		if (slave_handshake_p2_complete(shm, p) != UPERF_SUCCESS) {
			uperf_error("error in strand_init_all\n");
			slave_handshake_p2_failure("error in strand_init_all",
			    p, 0);
			free(shm);
			return (UPERF_FAILURE);

		}
	}

	if (preprocess_accepts(shm, g, &sl, 0) != UPERF_SUCCESS) {
		char msg[128];
		(void) snprintf(msg, sizeof (msg), "Error creating ports: %s",
			strerror(errno));
		uperf_error("%s\n", msg);
		slave_handshake_p2_failure(msg, p, 0);
		free(shm);
		return (UPERF_FAILURE);
	}

	if (shm_init_barriers_slave(shm, shm->worklist) != 0) {
		uperf_log_flush();
		uperf_fatal("Error initializing barriers"); /* NO RETURN */
	}

	(void) slave_spawn_strands(shm, p);

	if (slave_handshake_p2_success(sl, shm->tx_no_slave_info,
		p, shm->bitswap) != UPERF_SUCCESS) {
		uperf_log_flush();
		uperf_fatal("Error in completing handshake phase 2");
	}
	if (handshake_end_slave(sl, shm->tx_no_slave_info,
	    p, shm->bitswap) != UPERF_SUCCESS) {
		uperf_log_flush();
		uperf_fatal("Error in handshake end");
	}
	if (sl)
		free(sl);

	/* Finally, allow threads to start executing transactions */
	newstat_begin(0, AGG_STAT(shm), 0, 0);
	if ((error = slave_master_poll(shm, p)) != 0) {
		/* Kill threads on error */
		strand_killall(shm);
	}
	wait_for_strands(shm, error);
	newstat_end(0, AGG_STAT(shm), 0, 0);

	/*
	 * We can either send the UPERF_CMD_ABORT or goodbye_stat_t
	 * Either way, the master will abort the run
	 */
	(void) slave_master_goodbye(shm, p);

	uperf_log_flush();
	/* fprintf(stderr, "%ld: master-slave exiting\n", getpid()); */
	p->disconnect(p);
	free(shm);

	return (0);
}

/* Used by Slave to reap children */
static void
sigchld_signal(int signal)
{
	if (signal == SIGCHLD)
		reap_children++;
}

static int
setup_slave_signal()
{
	struct sigaction act;
	static sigset_t sigmask;

	(void) memset(&act, 0, sizeof (struct sigaction));
	act.sa_handler = sigchld_signal;
	act.sa_mask = sigmask;

	if (sigaction(SIGCHLD, &act, NULL) != 0) {
		perror("Sigchld");
		uperf_error("Unable to setup signal handler\n");
		return (UPERF_FAILURE);
	}

	if (sigaction(SIGPIPE, &act, NULL) != 0) {
		perror("Sigchld");
		uperf_error("Unable to setup signal handler\n");
		return (UPERF_FAILURE);
	}
	if (sigaction(SIGUSR2, &act, NULL) != 0) {
		perror("SIGUSR2");
		uperf_error("Unable to setup signal handler\n");
		return (UPERF_FAILURE);
	}

	return (UPERF_SUCCESS);
}


/*
 * Function: init_slave Description: 1. Initializes the slave 2. waits on
 * the well known port for incomming txn_groups 3. Accepts the txn_group
 * 4. Creates a worker thread that will serve the request
 */
int
slave()
{
	protocol_t	*conn;
	protocol_t	*slave_conn;

	uperf_log_init(&log);

	if (protocol_init(NULL) == UPERF_FAILURE) {
		return (-1);
	}

	if (setup_slave_signal() != 0) {
		return (1);
	}
	slave_conn = create_protocol(PROTOCOL_TCP, "", options.master_port, SLAVE);
	if (slave_conn == NULL) {
		uperf_error("Cannot create control connection\n");
		return (1);
	}
	if (slave_conn->listen(slave_conn, NULL) < 0) {
		uperf_log_flush();
		printf("Error starting slave\n");
		return (1);
	}
	/*
	 * Accept the incomming request and spawn a new worker thread to
	 * execute the request
	 */
	for (;;) {
		int status;
		conn = slave_conn->accept(slave_conn, NULL);
		if (conn == NULL) { /* timeout or poll is interrupted */
			uperf_log_flush();
			while (reap_children > 0) {
				int status;
				int pid = wait(&status);
				uperf_info("%d exited with status %d\n",
					pid, status);
				reap_children--;
			}
			continue;
		}
		status = fork();
		if (status == 0) {
			/* child */
			(void) slave_master(conn);
			exit(0);
		} else  {
			/* parent */
			uperf_info("forked one\n");
			destroy_protocol(conn->type, conn);
		}

	}
}
