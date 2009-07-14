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
#ifdef HAVE_ATOMIC_H
#include <atomic.h>
#endif /* HAVE_ATOMIC_H */
#include <assert.h>
#ifdef HAVE_SIGNAL_H
#include <signal.h> /* For SIGUSR2 */
#endif /* HAVE_SIGNAL_H */
#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include "uperf.h"
#include "sync.h"
#include "logging.h"
#include "main.h"
#include "flowops.h"
#include "workorder.h"
#include "strand.h"
#include "shm.h"
#include "rate.h"

extern options_t options;
typedef int (*generic_execute_func)(strand_t *, void *);

/*
 * Execute a function for specified duration
 */
static int
duration_execute(strand_t *sp, void *b, hrtime_t stop,
    generic_execute_func callback)
{
	int error = 0;

	if (STRAND_IS_LEADER(sp))
		shm_callout_register(sp->shmptr, stop, sp->worklist->groupid);

	/* We call "callback" multiple times as it might be a rate function */
	while (error == 0)
		error = callback(sp, b);

	return (error);
}

/* Returns one of UPERF_SUCCESS, UPERF_FAILURE, UPERF_DURATAION_EXPIRED */
static int
flowop_execute(strand_t *sp, flowop_t *fp)
{
	uint64_t i;
	int ret = 0;
	int save_errno;

	STATS_RECORD_FLOWOP(FLOWOP_BEGIN, sp, FLOWOP_STAT(fp), 0, 0);
	for (i = 0; i < fp->options.count; i++) {
		if (SIGNALLED(sp)) {
			return (UPERF_DURATION_EXPIRED);
		}
		ret = fp->execute(sp, fp);
		if (FO_CANFAIL(&fp->options) && ret < 0) {
			ret = 0;
		} else if (ret < 0)
			break;
	}
	save_errno = errno;
	STATS_RECORD_FLOWOP(FLOWOP_END, sp, FLOWOP_STAT(fp), ret, i);
	if ((save_errno == EINTR) || SIGNALLED(sp)) {
		return (UPERF_DURATION_EXPIRED);
	}
	return (ret >= 0 ? UPERF_SUCCESS : UPERF_FAILURE);
}

/* Returns one of UPERF_SUCCESS, UPERF_FAILURE, UPERF_DURATAION_EXPIRED */
static int
txn_execute_once(strand_t *strand, txn_t *txn)
{
	flowop_t *f;
	int ret = UPERF_SUCCESS;

	if (ENABLED_TXN_STATS(options)) {
		stats_update(TXN_BEGIN, strand, TXN_STAT(txn), 0, 0);
	}
	/* Execute flowops untill ERROR or DURATION_EXPIRED */
	for (f = txn->flist; f && ret == UPERF_SUCCESS; f = f->next) {
		ret = flowop_execute(strand, f);
	}
	if (ENABLED_TXN_STATS(options)) {
		stats_update(TXN_END, strand, TXN_STAT(txn), 0, 1);
	}
	return (ret);
}
/* Void * version of txn_execute_once. Used as callback for txn_duration() */
static int
txn_duration_callback(strand_t *sp, void *tp)
{
	return (txn_execute_once(sp, (txn_t *) tp));
}

static int
txn_rate_callback(void *a, void *b)
{
	return (txn_execute_once((strand_t *) a, (txn_t *) b));
}


static int
txn_execute_rate(strand_t *sp, void *tp)
{
	txn_t *txnp = (txn_t *) tp;
	assert(txnp->rate_count > 0);

	if (sp->shmptr->role == MASTER)
		return (rate_execute_1s(sp, txnp, txnp->rate_count,
						&txn_rate_callback));
	else
		return (rate_execute_1s_busywait(sp, txnp, txnp->rate_count,
						&txn_rate_callback));

}

/* Returns one of UPERF_SUCCESS, UPERF_FAILURE, UPERF_DURATAION_EXPIRED */
static int
txn_duration(strand_t *s, txn_t *txn)
{
	hrtime_t stop;
	generic_execute_func callback;

	stop = s->shmptr->txn_begin + txn->duration;

	if (txn->rate_count == 0)
		callback = &txn_duration_callback;
	else
		callback = &txn_execute_rate;

	return (duration_execute(s, txn, stop, callback));
}

static int
txn_iterations(strand_t *sp, txn_t *tp)
{
	int i;
	int error = UPERF_SUCCESS;

	for (i = 0; i < tp->iter && error == UPERF_SUCCESS; i++)
		error = txn_execute_once(sp, tp);

	return (error);
}

/* Returns one of UPERF_SUCCESS, UPERF_FAILURE, UPERF_DURATAION_EXPIRED */
static int
txn_execute(strand_t *strand, txn_t *txn)
{
	int error;

	if (txn->duration > 0) {
		error = txn_duration(strand, txn);
	} else {
		error = txn_iterations(strand, txn);
	}

	return (error);
}


/* Execute all transactions in a group.  */
int
group_execute(strand_t *strand, group_t *g)
{
	int error = 0;
	txn_t *txn;

	strand->buffer = (char *) calloc(1, group_max_dto_size(g));
	if (ENABLED_GROUP_STATS(options))
		stats_update(GROUP_BEGIN, strand, GROUP_STAT(g), 0, 0);
	for (txn = g->tlist; txn; txn = txn->next) {
		barrier_t *b = shm_get_barrier(strand->shmptr, g->groupid,
				    txn->txnid);
		strand->strand_state = STRAND_STATE_AT_BARRIER;
		wait_barrier(b);

		if (global_shm->global_error > 1) {
			break;
		}
		strand->strand_state = STRAND_STATE_EXECUTING;
		error = txn_execute(strand, txn);
		CLEAR_SIGNAL(strand);

		/*
		 * Possible values of error are success, failure and
		 * DURATION_EXPIRED. Duration expiry does not mean failure.
		 */
		if (error == UPERF_FAILURE) {
			break;
		}
	}
	strand->strand_state = STRAND_STATE_EXIT;
	if (ENABLED_GROUP_STATS(options))
		stats_update(GROUP_END, strand, GROUP_STAT(g), 0, 1);
	free(strand->buffer);

	return (error);
}
