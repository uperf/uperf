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
#include <liburing.h>
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

typedef struct {
	struct io_uring ring;
	int compl_cqes;
	unsigned int zc_tx_errors;
	bool zc_tx;
} tcp_zc_private_data;

static inline struct io_uring_cqe *wait_cqe_fast(struct io_uring *ring)
{
	struct io_uring_cqe *cqe;
	unsigned head;
	int ret;

	io_uring_for_each_cqe(ring, head, cqe)
		return cqe;

	ret = io_uring_wait_cqe(ring, &cqe);
	if (ret) {
		uperf_log_msg(UPERF_LOG_DEBUG, -ret, "wait cqe");
		errno = -ret;
		return NULL;
	}
	return cqe;
}

static int init_ring(protocol_t *p, flowop_options_t *flowop_options)
{
	int ring_flags = IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER;
	tcp_zc_private_data *pd = p->_protocol_p;
	int ret;

	ret = io_uring_queue_init(512, &pd->ring, ring_flags);
	if (ret) {
		uperf_log_msg(UPERF_LOG_ERROR, -ret, "io_uring init");
		return ret;
	}

	ret = io_uring_register_ring_fd(&pd->ring);
	if (ret < 0)
		uperf_log_msg(UPERF_LOG_ERROR, -ret, "register ring");

	pd->zc_tx = !FO_ZC_SKIP_TX(flowop_options);

	return UPERF_SUCCESS;
}

/* returns the port number */
static int
protocol_tcp_zc_listen(protocol_t *p, void *options)
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
protocol_tcp_zc_connect(protocol_t *p, void *options)
{
	struct sockaddr_storage serv;
	flowop_options_t *flowop_options = (flowop_options_t *)options;
	char msg[128];

	uperf_debug("tcp_zc: Connecting to %s:%d\n", p->host, p->port);

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
			    "tcp_zc: Enabling UDP encapsulation to port %d failed",
			    flowop_options->encaps_port);
			uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
			return (UPERF_FAILURE);
		}
#else
		(void) snprintf(msg, 128,
		    "tcp_zc: Enabling UDP encapsulation to port %d not supported",
		    flowop_options->encaps_port);
		uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
		return (UPERF_FAILURE);
#endif
	}
	if (generic_connect(p, &serv) < 0) {
		return (UPERF_FAILURE);
	}
	if (init_ring(p, flowop_options))
		return UPERF_FAILURE;
	return (UPERF_SUCCESS);
}

static int protocol_tcp_zc_send(protocol_t *p, void *buffer, int size,
                                void *options)
{
	tcp_zc_private_data *pd = p->_protocol_p;
	unsigned int msg_flags = MSG_WAITALL;
	const int notif_slack = 128;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	int ret;

	sqe = io_uring_get_sqe(&pd->ring);
	if (pd->zc_tx) {
		io_uring_prep_send_zc(sqe, p->fd, buffer, size, msg_flags, 0);
		sqe->ioprio = IORING_SEND_ZC_REPORT_USAGE;
	} else {
		io_uring_prep_send(sqe, p->fd, buffer, size, 0);
	}

	if (pd->compl_cqes >= notif_slack)
		ret = io_uring_submit_and_get_events(&pd->ring);
	else
		ret = io_uring_submit(&pd->ring);
	if (ret != 1) {
		uperf_log_msg(UPERF_LOG_ERROR, -ret, "io_uring submit");
		return ret;
	}

 complete:
	cqe = wait_cqe_fast(&pd->ring);
	if (!cqe)
		return -1;

	if (cqe->flags & IORING_CQE_F_NOTIF) {
		if (cqe->flags & IORING_CQE_F_MORE)
			uperf_log_msg(UPERF_LOG_ERROR, EINVAL, "F_MORE notif");
		if (cqe->res)
			pd->zc_tx_errors++;
		pd->compl_cqes--;
		io_uring_cqe_seen(&pd->ring, cqe);
		goto complete;
	}

	if (cqe->flags & IORING_CQE_F_MORE)
		pd->compl_cqes++;

	ret = cqe->res;
	if (ret < 0) {
		errno = -ret;
		ulog_warn("completion error");
	}
	io_uring_cqe_seen(&pd->ring, cqe);

	return ret;
}

static int protocol_tcp_zc_recv(protocol_t *p, void *buffer, int size,
                                void *options)
{
}

static protocol_t *protocol_tcp_zc_accept(protocol_t *p, void *options);

static protocol_t *
protocol_tcp_zc_new()
{
	protocol_t *newp;
	tcp_zc_private_data *new_tcp_zc_p;

	if ((newp = calloc(1, sizeof(protocol_t))) == NULL) {
		perror("calloc");
		return (NULL);
	}
	/* Allocating a local data structure */
	if ((new_tcp_zc_p = calloc(1, sizeof(*new_tcp_zc_p))) == NULL) {
		perror("calloc");
		return (NULL);
	}
	newp->connect = protocol_tcp_zc_connect;
	newp->disconnect = generic_disconnect;
	newp->listen = protocol_tcp_zc_listen;
	newp->accept = protocol_tcp_zc_accept;
	newp->write = protocol_tcp_zc_send;
	newp->send = protocol_tcp_zc_send;
	newp->read = generic_read;
	newp->recv = generic_recv;
	newp->wait = generic_undefined;
        newp->type = PROTOCOL_TCP_ZC;
	newp->_protocol_p = new_tcp_zc_p;
	(void) strlcpy(newp->host, "Init", MAXHOSTNAME);
	newp->fd = -1;
	newp->port = -1;
	newp->next = NULL;
	return (newp);
}

void
tcp_zc_fini(protocol_t *p)
{
	tcp_zc_private_data *pd;

	if (!p)
		return;
	pd = p->_protocol_p;
	if (!pd)
		return;

	while (pd->compl_cqes) {
		struct io_uring_cqe *cqe = wait_cqe_fast(&pd->ring);

		io_uring_cqe_seen(&pd->ring, cqe);
		pd->compl_cqes--;
	}
	io_uring_queue_exit(&pd->ring);

	if (pd->zc_tx_errors)
		ulog(UPERF_LOG_WARN, 0, "tcp_zc: zero-copy TX fell back to copying %u times",
		     pd->zc_tx_errors);

	free(pd);
	free(p);
}

static protocol_t *
protocol_tcp_zc_accept(protocol_t *p, void *options)
{
	protocol_t *newp;

	if ((newp = protocol_tcp_zc_new()) == NULL) {
		return (NULL);
	}
	if (generic_accept(p, newp, options) != 0) {
		return (NULL);
	}
	if (init_ring(newp, options)) {
		tcp_zc_fini(newp);
		return NULL;
	}
	return (newp);
}

protocol_t *
protocol_tcp_zc_create(char *host, int port)
{
	protocol_t *newp;

	if ((newp = protocol_tcp_zc_new()) == NULL) {
		return (NULL);
	}
	if (strlen(host) == 0) {
		(void) strlcpy(newp->host, "localhost", MAXHOSTNAME);
	} else {
		(void) strlcpy(newp->host, host, MAXHOSTNAME);
	}
        newp->port = port;

	uperf_debug("tcp_zc - Creating TCP ZC Protocol to %s:%d\n", host, port);
	return (newp);
}
