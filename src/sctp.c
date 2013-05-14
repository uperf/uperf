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

static void
set_sctp_options(int fd, flowop_options_t *f)
{
	struct sctp_initmsg init;

	if (f && FO_TCP_NODELAY(f)) {
		int nodelay = 1;
		if (setsockopt(fd, IPPROTO_SCTP, SCTP_NODELAY,
			(char *)&nodelay, sizeof (nodelay))) {
			uperf_log_msg(UPERF_LOG_WARN, errno,
			    "Cannot set SCTP_NODELAY");
		}
	}
	if (f && ((f->sctp_in_streams > 0) || (f->sctp_out_streams > 0))) {
		memset(&init, 0, sizeof(struct sctp_initmsg));
		init.sinit_max_instreams = f->sctp_in_streams;
		init.sinit_num_ostreams = f->sctp_out_streams;
		if (setsockopt(fd, IPPROTO_SCTP, SCTP_INITMSG, &init, (socklen_t)sizeof(struct sctp_initmsg)) < 0) {
			uperf_log_msg(UPERF_LOG_WARN, errno, "Cannot set sctp streams");
		}
	}
}

/* returns the port number */
/* ARGSUSED */
static int
protocol_listen(protocol_t *p, void *options)
{
	char msg[128];

	if (generic_socket(p, AF_INET6, IPPROTO_SCTP) != UPERF_SUCCESS) {
		if (generic_socket(p, AF_INET, IPPROTO_SCTP) != UPERF_SUCCESS) {
			(void) snprintf(msg, 128, "%s: Cannot create socket", "sctp");
			uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
			return (UPERF_FAILURE);
		}
	}
	set_sctp_options(p->fd, options);
	return (generic_listen(p, IPPROTO_SCTP));
}

static int
protocol_connect(protocol_t *p, void *options)
{
	struct sockaddr_storage serv;
	socklen_t len;
	const int off = 0;

	/*
	 * generic_connect() can't be used since the socket options
	 * must be set after calling socket() but before calling connect().
	 */
	uperf_debug("Connecting to %s:%d\n", p->host, p->port);

	if (name_to_addr(p->host, &serv)) {
		/* Error is already reported by name_to_addr, so just return */
		return (UPERF_FAILURE);
	}

	if (generic_socket(p, serv.ss_family, IPPROTO_SCTP) < 0) {
		return (UPERF_FAILURE);
	}
	set_sctp_options(p->fd, options);
	switch (serv.ss_family) {
	case  AF_INET:
		((struct sockaddr_in *)&serv)->sin_port = htons(p->port);
		len = (socklen_t)sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)&serv)->sin6_port = htons(p->port);
		len = (socklen_t)sizeof(struct sockaddr_in6);
		if (setsockopt(p->fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(int)) < 0) {
			return (UPERF_FAILURE);
		}
		break;
	default:
		uperf_debug("Unsupported protocol family: %d\n", serv.ss_family);
		return (UPERF_FAILURE);
		break;
	}
	if (connect(p->fd, (const struct sockaddr *)&serv, len) < 0) {
		ulog_err("%s: Cannot connect to %s:%d",
		         protocol_to_str(p->type), p->host, p->port);
		return (UPERF_FAILURE);
	}

	return (UPERF_SUCCESS);
}

int
sctp_write(protocol_t *p, void *buffer, int size, void *options)
#if defined(HAVE_SCTP_SENDV)
{
	ssize_t len;
	struct iovec iov;
	struct sctp_sendv_spa info;

	iov.iov_base = buffer;
	iov.iov_len = size;

	memset(&info, 0, sizeof(struct sctp_sendv_spa));
	if (options) {
		flowop_options_t *flowops;

		flowops = (flowop_options_t *) options;
		info.sendv_sndinfo.snd_sid = flowops->sctp_stream_id;
		if (FO_SCTP_UNORDERED(flowops)) {
			info.sendv_sndinfo.snd_flags |= SCTP_UNORDERED;
		}
		info.sendv_flags |= SCTP_SEND_SNDINFO_VALID;
#ifdef SCTP_PR_SCTP_TTL
		if (strcasecmp(flowops->sctp_pr_policy, "ttl") == 0) {
			info.sendv_prinfo.pr_policy = SCTP_PR_SCTP_TTL;
			info.sendv_prinfo.pr_value = flowops->sctp_pr_value;
			info.sendv_flags |= SCTP_SEND_PRINFO_VALID;
		}
#endif
#ifdef SCTP_PR_SCTP_RTX
		if (strcasecmp(flowops->sctp_pr_policy, "rtx") == 0) {
			info.sendv_prinfo.pr_policy = SCTP_PR_SCTP_RTX;
			info.sendv_prinfo.pr_value = flowops->sctp_pr_value;
			info.sendv_flags |= SCTP_SEND_PRINFO_VALID;
		}
#endif
#ifdef SCTP_PR_SCTP_BUF
		if (strcasecmp(flowops->sctp_pr_policy, "buf") == 0) {
			info.sendv_prinfo.pr_policy = SCTP_PR_SCTP_BUF;
			info.sendv_prinfo.pr_value = flowops->sctp_pr_value;
			info.sendv_flags |= SCTP_SEND_PRINFO_VALID;
		}
#endif
	}

	if ((len = sctp_sendv(p->fd, &iov, 1, NULL, 0,
	                      &info, sizeof(struct sctp_sendv_spa), SCTP_SENDV_SPA, 0)) < 0) {
		uperf_log_msg(UPERF_LOG_WARN, errno, "Cannot sctp_sendv ");
	}
	return (len);
}
#elif defined(HAVE_SCTP_SENDMSG)
{
	ssize_t len;
	uint32_t flags;
	uint16_t stream_no;
	uint32_t timetolive;

	flags = 0;
	timetolive = 0;
	if (options) {
		flowop_options_t *flowops;

		flowops = (flowop_options_t *) options;
		if (FO_SCTP_UNORDERED(flowops)) {
			flags |= SCTP_UNORDERED;
		}
		stream_no = flowops->sctp_stream_id;
		if (strcasecmp(flowops->sctp_pr_policy, "ttl") == 0) {
			timetolive = flowops->sctp_pr_value;
#if defined(UPERF_FREEBSD) || defined(UPERF_DARWIN)
#ifdef SCTP_PR_SCTP_TTL
			flags |= SCTP_PR_SCTP_TTL;
#endif
#endif
		}
#if defined(UPERF_FREEBSD) || defined(UPERF_DARWIN)
#ifdef SCTP_PR_SCTP_RTX
		if (strcasecmp(flowops->sctp_pr_policy, "rtx") == 0) {
			timetolive = flowops->sctp_pr_value;
			flags |= SCTP_PR_SCTP_RTX;
		}
#endif
#ifdef SCTP_PR_SCTP_BUF
		if (strcasecmp(flowops->sctp_pr_policy, "buf") == 0) {
			timetolive = flowops->sctp_pr_value;
			flags |= SCTP_PR_SCTP_BUF;
		}
#endif
#endif
	} else {
		stream_no = 0;
	}

	if ((len = sctp_sendmsg(p->fd, buffer, size, NULL, 0, htonl(0), flags,
	                        stream_no, timetolive, 0)) < 0) {
		uperf_log_msg(UPERF_LOG_WARN, errno, "Cannot sctp_sendmsg ");
	}
	return (len);
}
#else
{
	return (generic_write(p, buffer, size, options));
}
#endif

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
	newp->write = sctp_write;
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

