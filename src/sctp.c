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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#include <strings.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <assert.h>
#include "logging.h"
#include "uperf.h"
#include "flowops.h"
#include "workorder.h"
#include "protocol.h"
#include "generic.h"

#define	USE_POLL_ACCEPT	1
#define	LISTENQ		10240	/* 2nd argument to listen() */
#define	TCP_TIMEOUT	1200000	/* Argument to poll */
#define	SOCK_PORT(sin)	((sin).sin_port)

static void
set_sctp_options(int fd, flowop_options_t *f)
{

	if (f && FO_TCP_NODELAY(f)) {
		int nodelay = 1;
		if (setsockopt(fd, IPPROTO_SCTP, SCTP_NODELAY,
			(char *)&nodelay, sizeof (nodelay))) {
			uperf_log_msg(UPERF_LOG_WARN, errno,
			    "Cannot set SCTP_NODELAY");
		}
	}
}

/* returns the port number */
/* ARGSUSED */
static int
protocol_listen(protocol_t *p, void *options)
{
	if (generic_socket(p, IPPROTO_SCTP) != UPERF_SUCCESS) {
		return (UPERF_FAILURE);
	}
	return (generic_listen(p, IPPROTO_SCTP));
}

static int
protocol_connect(protocol_t *p, void *options)
{
	int status;

	status = generic_connect(p, IPPROTO_SCTP);
	if (status == UPERF_SUCCESS)
		set_sctp_options(p->fd, options);

	return (status);
}

static protocol_t *sctp_accept(protocol_t *p, void *options);

static protocol_t *
protocol_sctp_new()
{
	protocol_t *newp;

	newp = calloc(sizeof (protocol_t), 1);
	if (!newp)
		return (NULL);
	newp->connect = protocol_connect;
	newp->disconnect = generic_disconnect;
	newp->listen = protocol_listen;
	newp->accept = sctp_accept;
	newp->read = generic_read;
	newp->write = generic_write;
	newp->wait = generic_undefined;
	newp->type = PROTOCOL_SCTP;
	(void) strlcpy(newp->host, "Init", MAXHOSTNAME);
	newp->fd = -1;
	newp->port = -1;
	return (newp);
}

static protocol_t *
sctp_accept(protocol_t *p, void *options)
{
	protocol_t *newp;

	newp = protocol_sctp_new();
	if (generic_accept(p, newp, options) != UPERF_SUCCESS)
		return (NULL);
	set_sctp_options(newp->fd, options);

	return (newp);
}

protocol_t *
protocol_sctp_create(char *host, int port)
{
	protocol_t *p = protocol_sctp_new();

	(void) strlcpy(p->host, host, sizeof (p->host));
	if (strlen(host) == 0)
		(void) strlcpy(host, "localhost", MAXHOSTNAME);
	p->port = port;

	return (p);
}
