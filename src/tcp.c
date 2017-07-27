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
#include <netinet/tcp.h>
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

/* returns the port number */
static int
protocol_tcp_listen(protocol_t *p, void *options)
{
	flowop_options_t *flowop_options = (flowop_options_t *)options;
	char msg[128];

	/* SO_RCVBUF must be set before bind */

	if (generic_socket(p, AF_INET6, IPPROTO_TCP) != UPERF_SUCCESS) {
		if (generic_socket(p, AF_INET, IPPROTO_TCP) != UPERF_SUCCESS) {
			(void) snprintf(msg, 128, "%s: Cannot create socket", "tcp");
			uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
			return (UPERF_FAILURE);
		}
	}
	set_tcp_options(p->fd, flowop_options);

	return (generic_listen(p, IPPROTO_TCP, options));
}

static int
protocol_tcp_connect(protocol_t *p, void *options)
{
	struct sockaddr_storage serv;
	flowop_options_t *flowop_options = (flowop_options_t *)options;
	char msg[128];

	uperf_debug("tcp: Connecting to %s:%d\n", p->host, p->port);

	if (name_to_addr(p->host, &serv)) {
		/* Error is already reported by name_to_addr, so just return */
		return (UPERF_FAILURE);
	}
	if (generic_socket(p, serv.ss_family, IPPROTO_TCP) < 0) {
		return (UPERF_FAILURE);
	}
	set_tcp_options(p->fd, flowop_options);
	if ((flowop_options != NULL) && (flowop_options->encaps_port > 0)) {
		uperf_debug("Using UDP encapsulation with remote port %u\n",
		            flowop_options->encaps_port);
#ifdef TCP_REMOTE_UDP_ENCAPS_PORT
		if (setsockopt(p->fd, IPPROTO_TCP, TCP_REMOTE_UDP_ENCAPS_PORT,
		               &flowop_options->encaps_port, sizeof(int)) < 0) {
			(void) snprintf(msg, 128,
			    "tcp: Enabling UDP encapsulation to port %d failed",
			    flowop_options->encaps_port);
			uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
			return (UPERF_FAILURE);
		}
#else
		(void) snprintf(msg, 128,
		    "tcp: Enabling UDP encapsulation to port %d not supported",
		    flowop_options->encaps_port);
		uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
		return (UPERF_FAILURE);
#endif
	}
	if (generic_connect(p, &serv) < 0) {
		return (UPERF_FAILURE);
	}
	return (UPERF_SUCCESS);
}

static protocol_t *protocol_tcp_accept(protocol_t *p, void *options);

static protocol_t *
protocol_tcp_new()
{
	protocol_t *newp;

	if ((newp = calloc(1, sizeof(protocol_t))) == NULL) {
		perror("calloc");
		return (NULL);
	}
	newp->connect = protocol_tcp_connect;
	newp->disconnect = generic_disconnect;
	newp->listen = protocol_tcp_listen;
	newp->accept = protocol_tcp_accept;
	newp->read = generic_read;
	newp->write = generic_write;
	newp->send = generic_send;
	newp->recv = generic_recv;
	newp->wait = generic_undefined;
	newp->type = PROTOCOL_TCP;
	(void) strlcpy(newp->host, "Init", MAXHOSTNAME);
	newp->fd = -1;
	newp->port = -1;
	newp->next = NULL;
	return (newp);
}

static protocol_t *
protocol_tcp_accept(protocol_t *p, void *options)
{
	protocol_t *newp;

	if ((newp = protocol_tcp_new()) == NULL) {
		return (NULL);
	}
	if (generic_accept(p, newp, options) != 0) {
		return (NULL);
	}
	/*
	 * XXX I don't think setting the options is necessary, since
	 * it is done already on the listener and the options are inherited.
	 */
	if (options) {
		set_tcp_options(newp->fd, options);
	}
	return (newp);
}

protocol_t *
protocol_tcp_create(char *host, int port)
{
	protocol_t *newp;

	if ((newp = protocol_tcp_new()) == NULL) {
		return (NULL);
	}
	if (strlen(host) == 0) {
		(void) strlcpy(newp->host, "localhost", MAXHOSTNAME);
	} else {
		(void) strlcpy(newp->host, host, MAXHOSTNAME);
	}
	newp->port = port;
	uperf_debug("tcp - Creating TCP Protocol to %s:%d\n", host, port);
	return (newp);
}
