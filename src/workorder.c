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
#include <sys/types.h>
#ifdef HAVE_SYS_BYTEORDER_H
#include <sys/byteorder.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "uperf.h"
#include "flowops.h"
#include "workorder.h"

#define	UPERF_STOP_TXN		"Stop Transaction"
#define	UPERF_TXN_MASTER	"Txn End"
#define	UPERF_TXN_SLAVE		"OK"

#define	UPERF_SLAVE_READ_SIZE	64*1024

int
workorder_num_strands(workorder_t *w)
{
	int i;
	int count = 0;

	for (i = 0; i < w->ngrp; i++) {
		count += w->grp[i].nthreads;
	}

	return (count);
}

static int
group_num_stats(group_t *g)
{
	int count = 1;
	txn_t *t;

	for (t = g->tlist; t; t = t->next) {
		count = count + 1 + t->nflowop;
	}
	return (count);
}
int
workorder_num_stats(workorder_t *w)
{
	int i;
	int count = 0;

	for (i = 0; i < w->ngrp; i++) {
		count += group_num_stats(&w->grp[i]);
	}

	return (count);
}
int
workorder_num_strands_bytype(workorder_t *w, uint32_t type)
{
	int i;
	int count = 0;
	for (i = 0; i < w->ngrp; i++) {
		if (w->grp[i].strand_flag & type)
			count += w->grp[i].nthreads;
	}

	return (count);
}

int
workorder_max_txn(workorder_t *w)
{
	int i, no;

	no = 0;
	for (i = 0; i < w->ngrp; i++)
		no = MAX(no, w->grp[i].ntxn);

	return (no);
}
int
group_max_open_connections(group_t *g)
{
	int count = 0;
	txn_t *t;
	flowop_t *f;

	for (t = g->tlist; t; t = t->next) {
		int local_count = 1;
		for (f = t->flist; f; f = f->next) {
			if (f->type == FLOWOP_CONNECT ||
				f->type == FLOWOP_ACCEPT) {
					local_count += t->iter;
			} else if (f->type == FLOWOP_DISCONNECT) {
				local_count -= t->iter;
			}
		}
		count = MAX(count, local_count);
	}
	return (count);
}

int
workorder_num_connections(workorder_t *w)
{
	int i;
	int count = 0;

	for (i = 0; i < w->ngrp; i++) {
		group_t *g = &w->grp[i];
		count += g->nthreads * group_max_open_connections(g);
	}

	return (count);
}
int
group_max_dto_size(group_t *g)
{
	int count = DEFAULT_BUFFER_SIZE;
	txn_t *t;
	flowop_t *f;

	for (t = g->tlist; t; t = t->next) {
		for (f = t->flist; f; f = f->next) {
			if (f->options.size > count)
				count = f->options.size;
		}
	}

	return (count);
}

group_t *
group_clone(group_t *g1)
{
	txn_t *t1, *t2, *pt;
	flowop_t *f1, *f2, *pf;
	group_t *g2;

	if (g1 == NULL)
		return (NULL);

	g2 = malloc(sizeof (group_t));
	(void) memcpy(g2, g1, sizeof (group_t));
	pt = NULL;
	for (t1 = g1->tlist; t1; t1 = t1->next) {
		t2 = malloc(sizeof (txn_t));
		(void) memcpy(t2, t1, sizeof (txn_t));
		if (pt == NULL) {
			pt = t2;
			g2->tlist = t2;
		} else {
			pt->next = t2;
			pt = t2;
		}
		pf = NULL;
		for (f1 = t1->flist; f1; f1 = f1->next) {
			f2 = malloc(sizeof (flowop_t));
			(void) memcpy(f2, f1, sizeof (flowop_t));
			if (pf == NULL) {
				pf = f2;
				t2->flist = f2;
			} else {
				pf->next = f2;
				pf = f2;
			}
		}
	}
	return (g2);
}
void
group_free(group_t *g1)
{
	txn_t *t1, *t2;
	flowop_t *f1, *f2;

	if (g1 == NULL)
		return;
	t1 = g1->tlist;
	while (t1) {
		t2 = t1;
		f1 = t1->flist;
		while (f1) {
			f2 = f1;
			f1 = f1->next;
			free(f2);
		}
		t1 = t1->next;
		free(t2);
	}
	free(g1);
}

/*
 * We can be smart about what the slave does. 3 things
 * 1. If flowop is a read & has a count, just read size*count
 * 2. If read is the only flowop in the txn, and it the txn is
 *    using "iterations", just read size*iterations
 * 3. If read is the only flowop in the txn, and duration is used,
 *    try to read UPERF_SLAVE_READ_SIZE chunks at a time
 */
static void
optimize_txn_for_slave(txn_t *txn)
{
	int optimized = 0;
	flowop_t *f;

	for (f = txn->flist; f; f = f->next) {
		if (f->options.rsize > f->options.size) {
			flowop_options_t *o = &f->options;
			if (o->count > 1) {
				o->count = o->size * o->count/o->rsize;
			} else {
				optimized = o->size/o->rsize;
			}
			o->size = o->rsize;
		}
	}
	/* No need to manipulate the loop count if duration is used */
	if ((optimized == 0) || (txn->duration > 0))
		return;
	txn->iter /= optimized;
}

static int
option_oposite(flowop_options_t* option)
{
	uint16_t temp;

	temp = option->sctp_in_streams;
	option->sctp_in_streams = option->sctp_out_streams;
	option->sctp_out_streams = temp;
	return (UPERF_SUCCESS);
}

int
group_opposite(group_t *grp)
{
	txn_t *txn;
	flowop_t *fptr;

	for (txn = grp->tlist; txn; txn = txn->next) {
		for (fptr = txn->flist; fptr; fptr = fptr->next) {
			/* Fix up slave size for sendfilev */
			if ((fptr->type == FLOWOP_SENDFILEV) ||
			    (fptr->type == FLOWOP_SENDFILE)) {
				fptr->options.size = UPERF_SLAVE_READ_SIZE;
			}
			fptr->type = flowop_opposite(fptr->type);
			option_oposite(&fptr->options);
		}
		optimize_txn_for_slave(txn);
	}

	return (UPERF_SUCCESS);
}

int
group_bitswap(group_t *grp)
{
	txn_t *txn;
	flowop_t *fptr;

	grp->nthreads = BSWAP_32(grp->nthreads);
	/* No Need to swap ntxn; already done in rx_workorder */
	grp->max_async = BSWAP_32(grp->max_async);
	for (txn = grp->tlist; txn; txn = txn->next) {
		/* No need to swap nflowop; already done */
		txn->iter = BSWAP_64(txn->iter);
		txn->txnid = BSWAP_32(txn->txnid);
		txn->duration = BSWAP_64(txn->duration);
		txn->rate_count = BSWAP_32(txn->rate_count);
		for (fptr = txn->flist; fptr; fptr = fptr->next) {
			flowop_options_t *fo = &fptr->options;
			fo->size = BSWAP_32(fo->size);
			fo->rand_sz_min = BSWAP_32(fo->rand_sz_min);
			fo->rand_sz_max = BSWAP_32(fo->rand_sz_max);
			fo->flag = BSWAP_32(fo->flag);
			fo->protocol = BSWAP_32(fo->protocol);
			fo->port = BSWAP_32(fo->port);
			fo->nfiles = BSWAP_32(fo->nfiles);
			fo->count = BSWAP_64(fo->count);
			fo->wndsz = BSWAP_64(fo->wndsz);
			fo->poll_timeout = BSWAP_64(fo->poll_timeout);
			fo->duration = BSWAP_64(fo->duration);
		}
	}

	return (UPERF_SUCCESS);
}
