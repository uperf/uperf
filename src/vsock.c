/*
 * Copyright (C) 2021 Red Hat, Inc.  All rights reserved.
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

#include <limits.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef  HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif /*  HAVE_SYS_POLL_H */
#include <strings.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <sys/un.h>
#include <unistd.h>
#include "logging.h"
#include "uperf.h"
#include "flowops.h"
#include "workorder.h"
#include "protocol.h"
#include "generic.h"

/* Defined in <linux/vm_sockets.h> since Linux 5.8 */
#ifndef VMADDR_CID_LOCAL
#define VMADDR_CID_LOCAL 1
#endif

#define	LISTENQ		10240	/* 2nd argument to listen() */
//#define VSOCK_OVER_UDS  1	/* VSOCK over AF_UNIX support */

static void
set_vsock_options(int fd, flowop_options_t *f)
{
	if (f == NULL) {
		return;
	}

	if (f->wndsz > 0) {
		(void) generic_set_socket_buffer(fd, f->wndsz);
		(void) generic_verify_socket_buffer(fd, f->wndsz);
	}

	if (f && FO_NONBLOCKING(f)) {
		if (generic_setfd_nonblock(fd) != 0) {
			ulog_warn("non-blocking failed, falling back");
			CLEAR_FO_NONBLOCKING(f);
		}
	}

	return;
}

static int
protocol_vsock_sockaddr(struct sockaddr_storage *sas, socklen_t *len,
			const char *cid_str, unsigned int port, int listen)
{
	char *end = NULL;
	long cid;

	if (cid_str == NULL) {
		cid = VMADDR_CID_ANY;
	} else {
		cid = strtol(cid_str, &end, 10);
	}

	if (cid_str == NULL || (cid_str != end && *end == '\0')) {
		struct sockaddr_vm *svm = (struct sockaddr_vm *)sas;

		*len = sizeof(*svm);
		memset(svm, 0, *len);
		svm->svm_family = AF_VSOCK;
		svm->svm_cid = cid;
		if (port == ANY_PORT) {
			svm->svm_port = VMADDR_PORT_ANY;
		} else {
			svm->svm_port = port;
		}

		return (UPERF_SUCCESS);
	}

#ifdef VSOCK_OVER_UDS
	/*
	 * VSOCK over AF_UNIX
	 * cid_str can contain the UDS path
	 */
	struct sockaddr_un *sun = (struct sockaddr_un *)sas;

	*len = sizeof(*sun);
	memset(sun, 0, *len);
	sun->sun_family = AF_UNIX;
	strncpy(sun->sun_path, cid_str, sizeof(sun->sun_path) - 1);

	if (listen) {
		snprintf(sun->sun_path, sizeof(sun->sun_path), "%s_%u",
			 cid_str, port);
	} else {
		snprintf(sun->sun_path, sizeof(sun->sun_path), "%s", cid_str);
	}

	/* AF_UNIX path is not removed on socket close */
	if (listen) {
		(void)unlink(sun->sun_path);
	}

	return (UPERF_SUCCESS);
#else
	return (UPERF_FAILURE);
#endif /* VSOCK_OVER_UDS */

}

/* returns the port number */
/* ARGSUSED */
static int
protocol_vsock_listen(protocol_t *p, void *options)
{
	flowop_options_t *flowop_options = (flowop_options_t *)options;
	struct sockaddr_storage serv = {0};
	const int on = 1;
	socklen_t len;

	uperf_debug("%s: Binding on %s:%u\n",
		    protocol_to_str(p->type), p->host, p->port);

	/* NULL to listen on all CIDs */
	if (protocol_vsock_sockaddr(&serv, &len, NULL, p->port, 0) < 0) {
		return (UPERF_FAILURE);
	}

	if (generic_socket(p, serv.ss_family, 0) < 0) {
		return (UPERF_FAILURE);
	}
	set_vsock_options(p->fd, flowop_options);

	if (setsockopt(p->fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) < 0) {
		ulog_err("%s: Cannot set SO_REUSEADDR",
			protocol_to_str(p->type));
	}

	if (bind(p->fd, (const struct sockaddr *)&serv, len) < 0) {
		ulog_err("%s: Cannot bind to port %u",
		         protocol_to_str(p->type), p->port);
		return (UPERF_FAILURE);
	}

	if (serv.ss_family == AF_VSOCK && p->port == ANY_PORT) {
		struct sockaddr_vm *svm = (struct sockaddr_vm *)&serv;
		len = sizeof(*svm);

		memset(svm, 0, len);
		if ((getsockname(p->fd, (struct sockaddr *)svm, &len)) < 0) {
			ulog_err("%s: Cannot getsockname",
			         protocol_to_str(p->type));
			return (UPERF_FAILURE);
		}

		p->port = svm->svm_port;
	}

	listen(p->fd, LISTENQ);
	uperf_debug("%s: Listening on port %u\n",
		    protocol_to_str(p->type), p->port);

	return (p->port);
}

static int
protocol_vsock_connect(protocol_t *p, void *options)
{
	flowop_options_t *flowop_options = (flowop_options_t *)options;
	struct sockaddr_storage serv = {0};
	socklen_t len;

	uperf_debug("%s: Connecting to %s:%u\n",
		    protocol_to_str(p->type), p->host, p->port);

	if (protocol_vsock_sockaddr(&serv, &len, p->host, p->port, 0) < 0) {
		return (UPERF_FAILURE);
	}

	if (generic_socket(p, serv.ss_family, 0) < 0) {
		return (UPERF_FAILURE);
	}
	set_vsock_options(p->fd, flowop_options);

	if (connect(p->fd, (const struct sockaddr *)&serv, len) < 0) {
		ulog_err("%s: Cannot connect to %s:%u",
			 protocol_to_str(p->type), p->host, p->port);
		return (UPERF_FAILURE);
	}

#ifdef VSOCK_OVER_UDS
	/*
	 * VSOCK over AF_UNIX requires a little handshake as defined here:
	 * https://github.com/firecracker-microvm/firecracker/blob/master/docs/vsock.md
	 */
	if (serv.ss_family == AF_UNIX) {
		char buf[1024];

		/* Send "CONNECT $PORT\n" */
		snprintf(buf, 1024, "CONNECT %u\n", p->port);

		if (p->write(p, buf, strnlen(buf, 1024), NULL) < 0) {
			return (UPERF_FAILURE);
		}

		/* Receive "OK $REMOTE_PORT\n" */
		buf[0] = '\0';
		while (buf[0] != '\n') {
			if (p->read(p, buf, 1, NULL) <= 0) {
				return (UPERF_FAILURE);
			}
		}
	}
#endif /* VSOCK_OVER_UDS */

	return (UPERF_SUCCESS);
}

static protocol_t * protocol_vsock_new();

static protocol_t *
protocol_vsock_accept(protocol_t *p, void *options)
{
	struct sockaddr_storage remote;
	char hostname[NI_MAXHOST];
	socklen_t addrlen;
	protocol_t *newp;
	int timeout;

	if ((newp = protocol_vsock_new()) == NULL) {
		return (NULL);
	}

	addrlen = (socklen_t)sizeof(struct sockaddr_storage);
	timeout = 10000;

	if ((generic_poll(p->fd, timeout, POLLIN)) <= 0) {
		goto free;
	}

	if ((newp->fd = accept(p->fd, (struct sockaddr *)&remote,
	    &addrlen)) < 0) {
		ulog_err("accept:");
		goto free;
	}

	switch (remote.ss_family) {
	case AF_VSOCK:
	{
		struct sockaddr_vm *svm = (struct sockaddr_vm *)&remote;

		snprintf(hostname, sizeof(hostname), "%u", svm->svm_cid);
		newp->port = svm->svm_port;
		break;
	}
#ifdef VSOCK_OVER_UDS
	case AF_UNIX:
	{
		struct sockaddr_un *sun = (struct sockaddr_un *)&remote;

		snprintf(hostname, sizeof(hostname), "%s", sun->sun_path);
		newp->port = 0;
		break;
	}
#endif /* VSOCK_OVER_UDS */
	default:
		goto close;
	}

	(void) strlcpy(newp->host, hostname, sizeof(newp->host));
	uperf_info("Accepted connection from %s:%u\n", newp->host, newp->port);

	return (newp);
close:
	close(newp->fd);
free:
	free(newp);
	return NULL;
}

static protocol_t *
protocol_vsock_new()
{
	protocol_t *newp;

	if ((newp = calloc(1, sizeof(protocol_t))) == NULL) {
		perror("calloc");
		return (NULL);
	}
	newp->connect = protocol_vsock_connect;
	newp->disconnect = generic_disconnect;
	newp->listen = protocol_vsock_listen;
	newp->accept = protocol_vsock_accept;
	newp->read = generic_read;
	newp->write = generic_write;
	newp->wait = generic_undefined;
	newp->type = PROTOCOL_VSOCK;
	(void) strlcpy(newp->host, "Init", MAXHOSTNAME);
	newp->fd = -1;
	newp->port = -1;
	return (newp);
}

protocol_t *
protocol_vsock_create(char *host, int port)
{
	protocol_t *newp;

	if ((newp = protocol_vsock_new()) == NULL) {
		return (NULL);
	}
	if (strlen(host) == 0) {
		/* VMADDR_CID_LOCAL(1) is used for local communication */
		snprintf(newp->host, MAXHOSTNAME, "%u", VMADDR_CID_LOCAL);
	} else {
		strlcpy(newp->host, host, MAXHOSTNAME);
	}
	newp->port = port;
	uperf_debug("vsock - Creating VSOCK Protocol to %s:%d\n", host, port);
	return (newp);
}
