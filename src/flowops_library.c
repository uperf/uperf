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
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uperf.h"
#include "protocol.h"
#include "logging.h"
#include "main.h"
#include "flowops.h"
#include "workorder.h"
#include "stats.h"
#include "strand.h"
#include "shm.h"
#include "delay.h"
#include "sendfilev.h"

extern options_t options;

#define	UPERF_STOP_MSG		"Stop Writes"

typedef int (*flowop_rw_execute)(protocol_t *, void *buf, int sz, void *o);

/* Return a random number uniformly distributed between x and y */
static uint32_t
my_random(uint32_t x, uint32_t y)
{
	long range;
	double d;

	range = y - x + 1;
	d = x + rand() % range;
	if (d > y)
		d = y;
	return ((uint32_t) d);
}


static int
flowop_rw(strand_t *s, flowop_t *f)
{
	int n;
	int sz;
	flowop_rw_execute func = NULL;
	flowop_options_t *fo = &f->options;

	if (f->connection == NULL) {
		f->connection = strand_get_connection(s, f->p_id);
		if (f->connection == NULL) {
			char msg[1024];
			snprintf(msg, sizeof(msg), "No such connection %d", f->p_id);
			uperf_log_msg(UPERF_LOG_ERROR, 0, msg);
			return (-1);
		}
	}

	if (f->type == FLOWOP_READ)
		func = f->connection->read;
	else if (f->type == FLOWOP_WRITE)
		func = f->connection->write;
	else if (f->type == FLOWOP_SEND)
		func = f->connection->send;
	else if (f->type == FLOWOP_RECV)
		func = f->connection->recv;

	/* We only use rand_sz-* on transmit; fallback to rand_sz_max on rx */
	if (FO_RANDOM_SIZE(fo)) {
		if ((f->type == FLOWOP_WRITE) || (f->type == FLOWOP_SEND)) {
			fo->size = my_random(fo->rand_sz_min, fo->rand_sz_max);
		} else {
			fo->size = fo->rand_sz_max;
		}
	}

	if (func == NULL) {
		char msg[1024];
		snprintf(msg, sizeof(msg), "flowop %s not supported", f->name);
		uperf_log_msg(UPERF_LOG_ERROR, 0, msg);
		return (-1);
	}
	assert(fo->size > 0);
	sz = 0;
	while (sz < fo->size) {
		if (SIGNALLED(s))
			return (-1);
		n = func(f->connection, s->buffer + sz, fo->size - sz, fo);
		/*
		 * read(2) and write(2) can return 0 in case of a
		 * hangup. For this case, we just assume that the
		 * duration has expired and return
		 */
		if (n == 0) {
			errno = EINTR;
			return (-1);
		}
		if (n <= 0) {
			if (errno != EINTR) {
				int serrno = errno;
				char msg[1024];
				snprintf(msg, sizeof(msg), "Error for flowop %s ", f->name);
				uperf_log_msg(UPERF_LOG_ERROR, serrno, msg);
				/* snprintf could change errno */
				errno = serrno;
			}
			return (-1);
		}
		sz += n;
		if (FO_RANDOM_SIZE(fo))
			break;
	}

	return (sz);
}

/*
 * When we do a connect, we need to talk to the controller
 * ports for that protocol These controller ports can be
 * different based on the host we are talking to.
 */
int
flowop_connect(strand_t *sp, flowop_t *fp)
{
	int port;
	int error;
	protocol_t *datap;

	port = strand_get_port(sp, fp->options.remotehost,
			fp->options.protocol);
	if (port == -1) {
		char msg[1024];

		(void) snprintf(msg, sizeof (msg),
			"Cannot lookup host %s", fp->options.remotehost);
		uperf_log_msg(UPERF_LOG_ERROR, 0, msg);

		return (-1);
	}
	datap = create_protocol(fp->options.protocol,
			fp->options.remotehost, port, sp->role);
	if (datap == NULL)
		return (-1);
	datap->p_id = fp->p_id;

	error = datap->connect(datap, &fp->options);
	if (error == UPERF_SUCCESS) {
		strand_add_connection(sp, datap);
		/* mark following flowops in this txn that they need to get a new connection */
		for (flowop_t *fp1 = fp; fp1 != NULL; fp1 = fp1->next) {
			fp1->connection = NULL;
		}
	}
	else
		destroy_protocol(datap->type, datap);

	return (error);
}

/*
 * We might be disconnecting in a loop, so just disconnect
 * the first one
 */
int
flowop_disconnect(strand_t *sp, flowop_t *fp)
{
	int error;
	protocol_t *datap;

	datap = strand_get_connection(sp, fp->p_id);
	if (datap == NULL) {
		uperf_log_msg(UPERF_LOG_ERROR, 0, "Connection already closed");
		errno = EINTR;
		return (-1);
	}
	error = datap->disconnect(datap);
	strand_delete_connection(sp, fp->p_id);
	/* mark following flowops in this txn that they need to get a new connection */
	for (flowop_t *fp1 = fp; fp1 != NULL; fp1 = fp1->next) {
		fp1->connection = NULL;
	}

	return (error);
}

int
flowop_accept(strand_t *sp, flowop_t *fp)
{
	protocol_t *cntrp;
	protocol_t *newp;

	cntrp = sp->listen_conn[fp->options.protocol];
	assert(cntrp != NULL);
	assert(cntrp->accept != NULL);

	newp = cntrp->accept(cntrp, &fp->options);

	if (newp == NULL) {
		return (-1);
	}
	newp->p_id = fp->p_id;
	strand_add_connection(sp, newp);

	/* mark following flowops in this txn that they need to get a new connection */
	for (flowop_t *fp1 = fp; fp1 != NULL; fp1 = fp1->next) {
		fp1->connection = NULL;
	}
	return (0);
}

int
flowop_sendfilev(strand_t *s, flowop_t *f)
{
	int n = 0;

	if (f->connection == NULL) {
		f->connection = strand_get_connection(s, f->p_id);
		if (f->connection == NULL) {
			char msg[1024];
			snprintf(msg, sizeof(msg), "No such connection %d", f->p_id);
			uperf_log_msg(UPERF_LOG_ERROR, 0, msg);
			return (-1);
		}
	}

	if (f->type == FLOWOP_SENDFILEV)
#ifdef HAVE_SENDFILEV
		n = do_sendfilev(f->connection->fd, f->options.dir,
			f->options.nfiles, f->options.size);
#else
		assert("sendfilev not supported");
#endif
	else
		n = do_sendfile(f->connection->fd, f->options.dir,
			f->options.size);

	return (n);
}

/* ARGSUSED */
int
flowop_nop(strand_t *sp, flowop_t *fp)
{
	return (0);
}

/* ARGSUSED */
int
flowop_think(strand_t *sp, flowop_t *fp)
{
	int error;
	flowop_options_t *fo = &fp->options;
	uperf_info("thinking for %.2fms\n", 1.0*fp->options.duration/1.0e+6);
	if (FO_THINK_BUSY(fo)) {
		error = uperf_spin(fp->options.duration);
	} else {
		error = uperf_sleep(fp->options.duration);
	}

	return (error);
}

/* ARGSUSED */
int
flowop_unknown(strand_t *st, flowop_t *fp)
{
	char msg[128];

	snprintf(msg, sizeof (msg), "flowop - Unknown flowop %d", fp->type);
	uperf_log_msg(UPERF_LOG_ERROR, 0, msg);

	return (-1);
}

execute_func
flowop_get_execute_func(int type)
{
	execute_func func = &flowop_unknown;
	switch (type) {
	case FLOWOP_WRITE: /* FALLTHROUGH */
	case FLOWOP_READ:
	case FLOWOP_SEND:
	case FLOWOP_RECV:
		func = &flowop_rw;
		break;
	case FLOWOP_CONNECT:
		func = &flowop_connect;
		break;
	case FLOWOP_DISCONNECT:
		func = &flowop_disconnect;
		break;
	case FLOWOP_ACCEPT:
		func = &flowop_accept;
		break;
	case FLOWOP_NOP:
		func = &flowop_nop;
		break;
	case FLOWOP_THINK:
		func = &flowop_think;
		break;
	case FLOWOP_SENDFILEV:
	case FLOWOP_SENDFILE:
		func = &flowop_sendfilev;
		break;
	}

	return (func);
}
