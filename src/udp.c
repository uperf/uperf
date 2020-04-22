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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved. Use is subject
 * to license terms.
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#ifdef  HAVE_SYS_POLL_H
#include <sys/poll.h>
#include <netdb.h>
#endif /*  HAVE_SYS_POLL_H */

#include "uperf.h"
#include "protocol.h"
#include "logging.h"
#include "flowops.h"
#include "workorder.h"
#include "generic.h"

#define	UDP_TIMEOUT	5000
#define	UDP_HANDSHAKE	"uperf udp handshake"

typedef struct {
	int		sock;	/* for doing the communication */
	char		*rhost;
	int		port;
	int		refcount;
	struct sockaddr_storage addr_info;
} udp_private_data;

protocol_t *protocol_udp_create(char *host, int port);

static int
set_udp_options(int fd, flowop_options_t *f)
{
	if (f == NULL)
		return (0);

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

	return (0);
}

static int
read_one(int fd, char *buffer, int len, struct sockaddr_storage *from)
{
	int ret;
	socklen_t length = (socklen_t)sizeof(struct sockaddr_storage);
	/*
	 * uperf_debug("recvfrom %s:%d\n", inet_ntoa(saddr->sin_addr),
	 * saddr->sin_port);
	 */

	ret = recvfrom(fd, buffer, len, 0, (struct sockaddr *)from, &length);
	if (ret <= 0) {
		if (errno != EWOULDBLOCK)
			uperf_log_msg(UPERF_LOG_ERROR, errno, "recvfrom:");
		return (-1);
	}

	return (ret);
}

static int
write_one(int fd, char *buffer, int len, struct sockaddr *to)
{
	socklen_t length;

	switch (to->sa_family) {
	case AF_INET:
		length = (socklen_t)sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		length = (socklen_t)sizeof(struct sockaddr_in6);
		break;
	default:
		return (-1);
		break;
	}
	return (sendto(fd, buffer, len, 0, to, length));
}

static int
protocol_udp_read(protocol_t *p, void *buffer, int n, void *options)
{
	udp_private_data *pd = (udp_private_data *) p->_protocol_p;
	int ret;
	int nleft;
	int timeout = 0;
	flowop_options_t *fo = (flowop_options_t *)options;

	if (fo != NULL) {
		timeout = (int) fo->poll_timeout/1.0e+6;
	}
	/* HACK: Force timeout for UDP */
	/* if (timeout == 0) */
		/* timeout = UDP_TIMEOUT; */

	nleft = n;
	if (fo && FO_NONBLOCKING(fo)) {
		/*
		 * First try to read, if EWOULDBLOCK, then
		 * poll for fo->timeout seconds
		 */
		ret = read_one(pd->sock, buffer, n, &pd->addr_info);
		/* Lets fallback to poll/read */
		if ((ret <= 0) && (errno != EWOULDBLOCK)) {
			uperf_log_msg(UPERF_LOG_ERROR, errno,
			    "non-block write");
			return (ret);
		}
		nleft = n - ret;
	}
	if ((nleft > 0) && (timeout > 0)) {
		ret = generic_poll(pd->sock, timeout, POLLIN);
		if (ret > 0)
			return (read_one(pd->sock, buffer, nleft, &pd->addr_info));
		else
			return (-1); /* ret == 0 means timeout (error); */
	} else if ((nleft > 0) && (timeout <= 0)) {
		/* Vanilla read */
		return (read_one(pd->sock, buffer, nleft, &pd->addr_info));
	}
	assert(0); /* Not reached */

	return (UPERF_FAILURE);
}

/*
 * Function: protocol_udp_write Description: udp implementation of write We
 * cannot use send() as the server may not have done a connect()
 */
static int
protocol_udp_write(protocol_t *p, void *buffer, int n, void *options)
{
	udp_private_data *pd = (udp_private_data *) p->_protocol_p;
	int ret;
	size_t nleft;
	int timeout = 0;
	flowop_options_t *fo = (flowop_options_t *)options;

	if (fo != NULL) {
		timeout = (int) fo->poll_timeout/1.0e+6;
	}

	nleft = n;

	if (fo && FO_NONBLOCKING(fo)) {
		/*
		 * First try to write, if EWOULDBLOCK, then
		 * poll for fo->timeout seconds
		 */
		ret = write_one(pd->sock, buffer, n,
				(struct sockaddr *)&pd->addr_info);
		if ((ret <= 0) && (errno != EWOULDBLOCK)) {
			uperf_log_msg(UPERF_LOG_ERROR, errno,
			    "non-block write");
			return (-1);
		} else if (ret > 0) {
			nleft = n - ret;
		}
	}

	if ((nleft > 0) && (timeout > 0)) {
		ret = generic_poll(pd->sock, timeout, POLLOUT);
		if (ret > 0)
			return (write_one(pd->sock, buffer, nleft,
					(struct sockaddr *)&pd->addr_info));
		else
			return (-1);
	} else if ((nleft > 0) && (timeout <= 0)) {
		/* Vanilla write */
		return (write_one(pd->sock, buffer, nleft,
				(struct sockaddr *)&pd->addr_info));
	}
	assert(0);

	return (UPERF_FAILURE);
}

/*
 * Function: protocol_udp_listen Description: In UDP, there is no need for a
 * special "listen". All you need to do is to open the socket, you are then
 * in business
 */
/* ARGSUSED1 */
static int
protocol_udp_listen(protocol_t *p, void *options)
{
	udp_private_data *pd = (udp_private_data *)p->_protocol_p;
	socklen_t len;
	char msg[128];

	if ((pd->sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		if ((pd->sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			(void) snprintf(msg, 128, "%s: Cannot create socket", "udp");
			uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
			return (UPERF_FAILURE);
		} else {
			struct sockaddr_in *sin;

			sin = (struct sockaddr_in *)&pd->addr_info;
			memset(sin, 0, sizeof(struct sockaddr_in));
			sin->sin_family = AF_INET;
			sin->sin_port = htons(pd->port);
			sin->sin_addr.s_addr = htonl(INADDR_ANY);
			if (bind(pd->sock, (const struct sockaddr *)sin, sizeof(struct sockaddr_in)) < 0) {
				uperf_log_msg(UPERF_LOG_ERROR, errno, "bind");
				return (UPERF_FAILURE);
			} else {
				if (pd->port == ANY_PORT) {
					memset(sin, 0, sizeof(struct sockaddr_in));
					len = (socklen_t)sizeof(struct sockaddr_in);
					if ((getsockname(pd->sock, (struct sockaddr *)sin, &len)) < 0) {
						uperf_log_msg(UPERF_LOG_ERROR, errno, "getsockname");
						return (UPERF_FAILURE);
					} else {
						pd->port = ntohs(sin->sin_port);
					}
				}
			}
		}
	} else {
		struct sockaddr_in6 *sin6;
		const int off = 0;

		if (setsockopt(pd->sock, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(int)) < 0) {
			return (UPERF_FAILURE);
		}
		sin6 = (struct sockaddr_in6 *)&pd->addr_info;
		memset(sin6, 0, sizeof(struct sockaddr_in6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = htons(pd->port);
		sin6->sin6_addr = in6addr_any;
		if (bind(pd->sock, (const struct sockaddr *)sin6, sizeof(struct sockaddr_in6)) < 0) {
			uperf_log_msg(UPERF_LOG_ERROR, errno, "bind");
			return (UPERF_FAILURE);
		} else {
			if (pd->port == ANY_PORT) {
				memset(sin6, 0, sizeof(struct sockaddr_in6));
				len = (socklen_t)sizeof(struct sockaddr_in6);
				if ((getsockname(pd->sock, (struct sockaddr *)sin6, &len)) < 0) {
					uperf_log_msg(UPERF_LOG_ERROR, errno, "getsockname");
					return (UPERF_FAILURE);
				} else {
					pd->port = ntohs(sin6->sin6_port);
				}
			}
		}
	}
	uperf_debug("Listening on port %d\n", pd->port);
	return (pd->port);
}

static protocol_t *
protocol_udp_accept(protocol_t *p, void *options)
{
	char msg[32];
	char hostname[NI_MAXHOST];
	int port;
	udp_private_data *pd = (udp_private_data *)p->_protocol_p;
	flowop_options_t *fo = (flowop_options_t *)options;

	(void) bzero(msg, sizeof (msg));
	if ((protocol_udp_read(p, msg, strlen(UDP_HANDSHAKE), fo) <=
		UPERF_SUCCESS)) {
		uperf_log_msg(UPERF_LOG_ERROR, errno,
		    "Error in UDP Handshake");
		uperf_info("\nError in UDP Handshake\n");
		return (NULL);
	}

	(void) set_udp_options(pd->sock, (flowop_options_t *)options);
	if (strcmp(msg, UDP_HANDSHAKE) != 0)
		return (NULL);
	pd->refcount++;
	switch (pd->addr_info.ss_family) {
	case AF_INET:
	{
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)&pd->addr_info;
		inet_ntop(AF_INET, &sin->sin_addr, hostname, sizeof(hostname));
		port = ntohs(sin->sin_port);
		break;
	}
	case AF_INET6:
	{
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)&pd->addr_info;
		inet_ntop(AF_INET6, &sin6->sin6_addr, hostname, sizeof(hostname));
		port = ntohs(sin6->sin6_port);
		break;
	}
	default:
		return (NULL);
		break;
	}
	uperf_info("Handshake[%s] with %s:%d\n", msg, hostname, port);
	return (p);
}

static int
protocol_udp_connect(protocol_t *p, void *options)
{
	udp_private_data *pd = (udp_private_data *)p->_protocol_p;
	flowop_options_t *fo = (flowop_options_t *)options;
	const int off = 0;

	(void) memset(&pd->addr_info, 0, sizeof(struct sockaddr_storage));
	if (name_to_addr(pd->rhost, &pd->addr_info) != 0) {
		/* Error is already reported by name_to_addr, so just return */
		return (UPERF_FAILURE);
	}
	if ((pd->sock = socket(pd->addr_info.ss_family, SOCK_DGRAM, 0)) < 0) {
		ulog_err("%s: Cannot create socket", protocol_to_str(p->type));
		return (UPERF_FAILURE);
	}
	switch (pd->addr_info.ss_family) {
	case AF_INET:
		((struct sockaddr_in *)&pd->addr_info)->sin_port = htons(pd->port);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)&pd->addr_info)->sin6_port = htons(pd->port);
		if (setsockopt(pd->sock, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(int)) < 0) {
			return (UPERF_FAILURE);
		}
		break;
	default:
		uperf_debug("Unsupported protocol family: %d\n", pd->addr_info.ss_family);
		return (UPERF_FAILURE);
		break;
	}
	(void) set_udp_options(pd->sock, fo);
	if ((protocol_udp_write(p, UDP_HANDSHAKE, strlen(UDP_HANDSHAKE),
		    NULL)) <= 0) {
		uperf_log_msg(UPERF_LOG_ERROR, errno, "Error in UDP Handshake");
		uperf_info("\nError in UDP Handshake\n");
		return (UPERF_FAILURE);
	}
	return (UPERF_SUCCESS);
}

static int
protocol_udp_disconnect(protocol_t *p)
{
	udp_private_data *pd = (udp_private_data *) p->_protocol_p;
	uperf_debug("udp - disconnect done\n");

	pd->refcount--;

	return (UPERF_SUCCESS);
}

protocol_t *
protocol_udp_create(char *host, int port)
{
	protocol_t *newp;
	udp_private_data *new_udp_p;

	if ((newp = calloc(1, sizeof(protocol_t))) == NULL) {
		perror("calloc");
		return (NULL);
	}
	if ((new_udp_p = calloc(1, sizeof(udp_private_data))) == NULL) {
		perror("calloc");
		return (NULL);
	}
	newp->connect = &protocol_udp_connect;
	newp->disconnect = &protocol_udp_disconnect;
	newp->read = &protocol_udp_read;
	newp->write = &protocol_udp_write;
	newp->listen = &protocol_udp_listen;
	newp->accept = &protocol_udp_accept;
	newp->wait = &generic_undefined;
	new_udp_p->rhost = strdup(host);
	new_udp_p->port = port;
	newp->type = PROTOCOL_UDP;
	newp->_protocol_p = new_udp_p;
	new_udp_p->refcount = 0;
	uperf_debug("udp - Creating UDP Protocol to %s:%d\n", host, port);
	return (newp);
}

void
udp_fini(protocol_t *p)
{
	udp_private_data *pd;
	static int called = 0;
	called++;
	if (!p)
		return;
	pd = (udp_private_data *) p->_protocol_p;
	if (!pd)
		return;
	/*
	 * In UDP, we use only one socket to communicate. If
	 * the master does multiple connects(), they are converted
	 * to do UDP_HANDSHAKE on this socket. Lets assume that the
	 * user wants to do 10 connects followed by 10 disconnects.
	 * Here, the 10 connects translates to 10 UDP_HANDSHAKEs.
	 * When the user does a disconnect, the normal thing to do
	 * is to close the socket. However, in UDP it will cause the
	 * following close() to fail, as the socket is already closed.
	 * We can use a refcount to track number of connects and close
	 * only when the refcount reaches 0.
	 */
	if (pd->refcount < -1) {
		if (pd->sock > 0)
			(void) close(pd->sock);
		free(pd->rhost);
		free(pd);
		free(p);
	}
}
