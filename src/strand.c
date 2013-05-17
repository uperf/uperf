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

#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <strings.h>
#include <assert.h>

#include "uperf.h"
#include "logging.h"
#include "workorder.h"
#include "strand.h"
#include "shm.h"
#include "delay.h"
#include "main.h"
#include "signals.h"

int group_execute(strand_t *, group_t *);
extern options_t options;

/*
 * This function is called by the main thread to initialize an
 * individual strand_t structure with the appropiate values
 * ssid is the thread id
 */
int
strand_init_group(uperf_shm_t *shm, group_t *g, int ssid)
{
	int j;
	protocol_t **pptr;

	uperf_debug("%s: ssid=%d\n", __func__, ssid);
	pptr = shm->connection_list;

	for (j = 0; j < g->nthreads; j++) {
		strand_t *st = shm_get_strand(shm, j + ssid);
		st->shmptr = shm;
		int nop = group_max_open_connections(g);

		(void) bzero(st, sizeof (strand_t));
		snprintf(STRAND_STAT(st)->name, UPERF_NAME_LEN, "Thr%d", j);
		STRAND_STAT(st)->type = NSTAT_STRAND;
		(void) bzero(st->ccache, sizeof (st->ccache));
		st->cpool = NULL;
		st->ccache_size = 0;

		if (j == 0)
			st->strand_flag |= STRAND_LEADER;

		pptr += nop;
	}
	shm->connection_list = pptr;

	return (UPERF_SUCCESS);
}


/*
 * This function is called by the main thread to initialize all
 * strand_t structures
 */
int
strand_init_all(uperf_shm_t *shm, workorder_t *w)
{
	int i;
	int count;

	count = 0;
	for (i = 0; i < w->ngrp; i++) {
		if (strand_init_group(shm, &w->grp[i], count)
		    != UPERF_SUCCESS) {
			return (UPERF_FAILURE);
		}
		count += w->grp[i].nthreads;
	}
	return (UPERF_SUCCESS);
}

int
strand_add_slave(strand_t *s, slave_info_t *si)
{
	slave_info_list_t *p = calloc(1, sizeof (slave_info_list_t));
	memcpy(&p->slave, si, sizeof (slave_info_t));

	/* Insert at beginning */
	p->next = s->slave_list;
	s->slave_list = p;

	return (0);
}

int
strand_get_port(strand_t *s, char *host, int protocol)
{
	slave_info_list_t *q = s->slave_list;

	while (q) {
		if ((strcmp(q->slave.host, host)) == 0) {
			return (q->slave.port[protocol]);
		}
		q = q->next;
	}
	/* Not found */
	return (-1);
}

int
strand_add_connection(strand_t *s, protocol_t *p)
{
	p->next = s->cpool;
	if (s->cpool)
		s->cpool->prev = p;
	s->cpool = p;
	return (0);
}

int
strand_delete_connection(strand_t *s, int id)
{
	protocol_t *ptr = s->cpool;
	while (ptr) {
		if (ptr->p_id == id) {
			if (ptr->prev)
				ptr->prev->next = ptr->next;
			if (ptr->next)
				ptr->next->prev = ptr->prev;
			if (ptr == s->cpool)
				s->cpool = ptr->next;
			destroy_protocol(ptr->type, ptr);
			s->ccache_size = 0;	 /* flush cache */
			return (0);
		}
		ptr = ptr->next;
	}
	return (1);
}

void
strand_put_connection_in_cache(strand_t *s, protocol_t *p)
{
	static int current_replacement = 0;

	if (s->ccache_size == STRAND_CONNECTION_CACHE_SIZE - 1) {
		s->ccache[current_replacement] = p;
		current_replacement
		    = (current_replacement + 1)%STRAND_CONNECTION_CACHE_SIZE;
	} else {
		s->ccache[s->ccache_size++] = p;
	}
}

/*
 * Get connection for id. If id == UPERF_ANY_CONNECTION, return
 * the first anonymous connection
 */
protocol_t *
strand_get_connection(strand_t *s, int id)
{
	int i;
	protocol_t *ptr = s->cpool;

	assert(s);
	if (!s->cpool)
		return (NULL);

	/* check cache */
	for (i = 0; i < s->ccache_size; i++) {
		if (s->ccache[i] == NULL)
			continue;
		if ((s->ccache[i]->p_id == id) || (id == -1)) {
			return (s->ccache[i]);
		}
	}
	/* cache miss */
	while (ptr) {
		if (ptr->p_id == id) {
			/* Match found, fill in cache before returning */
			strand_put_connection_in_cache(s, ptr);
			return (ptr);
		}
		ptr = ptr->next;
	}
	printf("No such connection with id %d\n", id);
	return (NULL);
}

/* Called by strand on exit */
void
strand_fini(strand_t *s)
{
	int i;
	protocol_t *p = s->cpool;
	protocol_t *ptmp;
	slave_info_list_t *sil;

	/* Make sure strand is dead */
	for (i = 0; i < NUM_PROTOCOLS; i++) {
		if (s->listen_conn[i] != 0) {
			protocol_t *p = s->listen_conn[i];
			/* FIXME: need to do reference counting */
			destroy_protocol(p->type, p);
			s->listen_conn[i] = 0;

		}
	}
	/* Close any open connections */
	while (p) {
		ptmp = p->next;
		destroy_protocol(p->type, p);
		p = ptmp;
	}
	sil = s->slave_list;
	while (sil) {
		slave_info_list_t *q = sil->next;
		free(sil);
		sil = q;
	}
	group_free(s->worklist);
}

/*
 * Wait for all the strands to terminate and join
 * An "error" indicator is passed to this because,
 * an error can be due to a thread getting stuck in a blocked
 * system call, and not being able to exit.
 * Since we will be calling exit(2) pretty soon, there is no
 * need to join (only thread).
 * For processes, we wait even on error since there is no possibility
 * that the process has not exited.
 */
void
wait_for_strands(uperf_shm_t *shm, int error)
{
	int i;
	static int joined = 0;

	if (joined == 1)
		return;

	for (i = 0; i < shm->no_strands; i++) {
		strand_t *s = shm_get_strand(shm, i);

		if (STRAND_IS_PROCESS(s)) {
			pid_t pid;
			int status;

			pid = wait(&status);
			if (status != 0) {
				uperf_info("pid %d exited with status %d\n",
					pid, status >> 8);
				flag_error("unknown exit code");
			}

		} else {
			/* if some error occurs, do NOT wait. */
			if (!error)
				if (pthread_join(s->tid, 0) != 0) {
					uperf_log_msg(UPERF_LOG_ERROR, errno,
					    "pthread join");
				}
		}
	}
	joined = 1;
}

int
signal_strand(strand_t *s, int signal)
{
	int status;

	if (STRAND_IS_PROCESS(s)) {
		pid_t pid =  s->pid;
		uperf_info("Sending signal %d(%s) to %d\n", signal,
			signal == SIGKILL ? "SIGKILL": "SIGUSR2",
			pid);
#ifndef UPERF_SOLARIS
		status = kill(pid, signal);
#else
		status = sigsend(P_PID, pid, signal);
#endif
		/* Ignore error if process has already exited */
		if (status == -1 && errno != ESRCH) {
			perror("sigsend");
		}
	} else {
		/* SIGKILL & pthreads do not like each other, use SIGUSR1 */
		if (signal == SIGKILL) {
			signal = SIGUSR1;
		}

		uperf_info("Sending signal %s to %lu\n",
			signal == SIGUSR1 ? "SIGUSR1(kill)": "SIGUSR2",
			(unsigned long)s->tid);
		errno = 0;
		if ((status = pthread_kill(s->tid, signal)) != 0) {
			if (status != ESRCH) {
				/*
				 * Strangely, the error is not
				 * reported in errno, but in the
				 * return value
				 */
				uperf_error("pthread_kill: err = %d\n",
				    status);
			}
		}
	}

	return (status);
}

#ifdef	DEBUG
void
print_stacks(uperf_shm_t *shm)
{
	int i;
	char cmd[128];
	for (i = 0; i < shm->no_strands; i++) {
		strand_t *s = shm_get_strand(shm, i);
		if (s->signalled == 1) {
			if (STRAND_IS_PROCESS(s)) {
				snprintf(cmd, 128, "/usr/bin/pstack %d",
					s->pid);
			} else {
				snprintf(cmd, 128, "/usr/bin/pstack %d/%lu",
					getpid(), (unsigned long)s->tid);
			}
			system(cmd);
			printf("Strand is at %d\n", s->strand_state);
		}
	}
}
#endif /* DEBUG */
#define	SIGNAL_SLEEP	200000000

int
strand_killall(uperf_shm_t *shm)
{
	int i;
	for (i = 0; i < shm->no_strands; i++) {
		strand_t *s = shm_get_strand(shm, i);
		signal_strand(s, SIGKILL);
	}
	return (0);
}

/*
 * Send a signal to all strands for a group. If group id is -1,
 * the signal is sent to all strands
 * Returns 0 on success, else 1 on error
 */
int
signal_all_strands(uperf_shm_t *shm, int groupid, int signal)
{
	int i;
	int no_signalled;
	int retries = REPEATED_SIGNAL_RETRIES;

	for (i = 0; i < shm->no_strands; i++) {
		strand_t *s = shm_get_strand(shm, i);
		if (STRAND_AT_BARRIER(s))
			continue;
		if ((groupid != -1) && (s->worklist->groupid != groupid))
			continue;
		s->signalled = 1;
	}

	do {
		no_signalled = 0;
		if (retries < REPEATED_SIGNAL_RETRIES)
			uperf_sleep(SIGNAL_SLEEP); /* give them a breather */
		for (i = 0; i < shm->no_strands; i++) {
			strand_t *s = shm_get_strand(shm, i);
			if (STRAND_AT_BARRIER(s))
				continue;
			if (s->signalled == 1) {
				if (signal_strand(s, signal) == 0) {
					no_signalled++;
				}
			}
		}
	} while ((no_signalled > 0) && (retries-- > 0));

	if (retries < 0 && no_signalled > 0) {
		int not_responding = 0;
		for (i = 0; i < shm->no_strands; i++) {
			strand_t *s = shm_get_strand(shm, i);
			if (STRAND_AT_BARRIER(s))
				not_responding++;
		}
		if (not_responding > 0) {
			uperf_info("%d threads not responding\n", no_signalled);
#ifdef	DEBUG
			print_stacks(shm);
#endif /* DEBUG */
			return (1);
		}
	}

	return (0);
}

/*
 * Thread/Process start routine
 */
void *
strand_run(void *sp)
{
	strand_t *s;
	uperf_shm_t *shm;
	int error;

	shm = global_shm;
	s = (strand_t *) sp;

	assert(sp);
	assert(shm);
	assert(s);

	if (shm->global_error > 0) {
		return (NULL);
	}

	if (setup_strand_signal() != UPERF_SUCCESS) {
		uperf_error("Cannot setup signal handlers\n");
		return (NULL);
	}
	/* set the pid of the process, will be used in accessing the /proc */
	s->pid = getpid();
	if ((shm->role == MASTER) && ENABLED_HISTORY_STATS(options)) {
		history_init(s);
	}

#ifdef USE_CPC
#define	ERRSTR	"*** Will not measure CPU counters for this run\n"
	if (ENABLED_STATS(options) && ENABLED_CPUCOUNTER_STATS(options)) {
		if (hwcounter_initlwp(&s->hw, options.ev1, options.ev2) == -1) {
			uperf_info(ERRSTR);
			options.copt &= (~CPUCOUNTER_STATS);
		}
	}
#endif

	/* Start transactions */
	newstat_begin(0, STRAND_STAT(s), 0, 0);
	error = group_execute(s, s->worklist);
	newstat_end(0, STRAND_STAT(s), 0, 1);
	if (error != UPERF_SUCCESS && error != EINTR) {
		flag_error("Error executing transactions");
	}

	shm_update_strand_exit(shm);
	if (ENABLED_HISTORY_STATS(options)) {
		flush_history(s);
	}
	strand_fini(s);

	return (NULL);
}
