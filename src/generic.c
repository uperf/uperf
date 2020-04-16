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
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#ifdef  HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif /*  HAVE_SYS_POLL_H */

#include "logging.h"
#include "uperf.h"
#include "flowops.h"
#include "workorder.h"
#include "protocol.h"

#define	USE_POLL_ACCEPT	1
#define	LISTENQ		10240	/* 2nd argument to listen() */
#define	TIMEOUT		1200000	/* Argument to poll */

int
name_to_addr(const char *address, struct sockaddr_storage *saddr)
{
	struct addrinfo *res, *res0, hints;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if ((error = getaddrinfo(address, NULL, NULL, &res0)) == 0) {
		for (res = res0; res; res = res->ai_next) {
			if ((res->ai_addr->sa_family != AF_INET) &&
			    (res->ai_addr->sa_family != AF_INET6)) {
				continue;
			}
			memcpy(saddr, res->ai_addr, res->ai_addrlen);
			break;
		}
		freeaddrinfo(res0);
		if (res != NULL) {
			return (UPERF_SUCCESS);
		} else {
			return (UPERF_FAILURE);
		}
	} else {
		ulog_err("name_to_addr(%s): %s\n", address, gai_strerror(error));
		return (UPERF_FAILURE);
	}
}

int
generic_socket(protocol_t *p, int domain, int protocol)
{
	if ((p->fd = socket(domain, SOCK_STREAM, protocol)) < 0) {
		ulog_err("%s: Cannot create socket", protocol_to_str(p->type));
		return (UPERF_FAILURE);
	}
	return (UPERF_SUCCESS);
}

/* ARGSUSED */
int
generic_connect(protocol_t *p, struct sockaddr_storage *serv)
{
	socklen_t len;
	const int off = 0;

	uperf_debug("Connecting to %s:%d\n", p->host, p->port);

	switch (serv->ss_family) {
	case AF_INET:
		((struct sockaddr_in *)serv)->sin_port = htons(p->port);
		len = (socklen_t)sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)serv)->sin6_port = htons(p->port);
		len = (socklen_t)sizeof(struct sockaddr_in6);
		if (setsockopt(p->fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(int)) < 0) {
			return (UPERF_FAILURE);
		}
		break;
	default:
		uperf_debug("Unsupported protocol family: %d\n", serv->ss_family);
		return (UPERF_FAILURE);
		break;
	}
	if (connect(p->fd, (const struct sockaddr *)serv, len) < 0) {
		ulog_err("%s: Cannot connect to %s:%d",
		         protocol_to_str(p->type), p->host, p->port);
		return (UPERF_FAILURE);
	}

	return (UPERF_SUCCESS);
}

int
generic_setfd_nonblock(int fd)
{
	return (fcntl(fd, F_SETFL, O_NONBLOCK));
}

int
generic_set_socket_buffer(int fd, int size)
{
	int w = size;

	if (w == 0)
		return (UPERF_SUCCESS);
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&w, sizeof (w))
		!= 0) {
		ulog_warn("Cannot set SO_SNDBUF");
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&w, sizeof (w))
		!= 0) {
		ulog_warn("Cannot set SO_RCVBUF");
	}

	return (UPERF_SUCCESS);
}

int
generic_verify_socket_buffer(int fd, int wndsz)
{
	int nwsz;
	socklen_t len;
	float diff;

	if (wndsz == 0)
		return (UPERF_SUCCESS);

	/* Now verify */
	len = sizeof (wndsz);
	nwsz = -1;
	if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&nwsz, &len) != 0) {
		ulog_warn("Cannot get SO_SNDBUF");
	}
	diff = 1.0*nwsz/wndsz;
	if (diff < 0.9 || diff > 1.1) {
		ulog(UPERF_LOG_WARN, 0, "%s: %.2fKB (Requested:%.2fKB)",
		          "Send buffer", nwsz/1024.0, wndsz/1024.0);
	} else {
		uperf_info("Set Send buffer size to %.2fKB\n", nwsz/1024.0);
	}

	len = sizeof (wndsz);
	if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&nwsz, &len) != 0) {
		ulog_warn("Cannot get SO_RCVBUF");
	}

	diff = 1.0*nwsz/wndsz;
	if (diff < 0.9 || diff > 1.1) {
		ulog(UPERF_LOG_WARN, 0, "%s: %.2fKB (Requested:%.2fKB)",
		          "Recv buffer", nwsz/1024.0, wndsz/1024.0);
	} else {
		uperf_info("Set Recv buffer size to %.2fKB\n", nwsz/1024.0);
	}

	return (UPERF_SUCCESS);
}

/* ARGSUSED */
int
generic_listen(protocol_t *p, int pflag, void* options)
{
	const int on = 1;
	const int off = 0;
	int use_ipv6_socket;
	socklen_t len;

	if (setsockopt(p->fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) < 0) {
		ulog_err("%s: Cannot set SO_REUSEADDR",
			protocol_to_str(p->type));
	}
	if (setsockopt(p->fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(int)) < 0) {
		use_ipv6_socket = 0;
	} else {
		use_ipv6_socket = 1;
	}
	if (use_ipv6_socket) {
		struct sockaddr_in6 sin6;

		memset(&sin6, 0, sizeof(struct sockaddr_in6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_port = htons(p->port);
		sin6.sin6_addr = in6addr_any;
		if (bind(p->fd, (const struct sockaddr *)&sin6, sizeof(struct sockaddr_in6)) < 0) {
			ulog_err("%s: Cannot bind to port %d",
			         protocol_to_str(p->type), p->port);
			return (UPERF_FAILURE);
		} else {
			if (p->port == ANY_PORT) {
				memset(&sin6, 0, sizeof(struct sockaddr_in6));
				len = (socklen_t)sizeof(struct sockaddr_in6);
				if ((getsockname(p->fd, (struct sockaddr *)&sin6, &len)) < 0) {
					ulog_err("%s: Cannot getsockname",
					         protocol_to_str(p->type));
					return (UPERF_FAILURE);
				} else {
					p->port = ntohs(sin6.sin6_port);
				}
			}
		}
	} else {
		struct sockaddr_in sin;

		memset(&sin, 0, sizeof(struct sockaddr_in));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(p->port);
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(p->fd, (const struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
			ulog_err("%s: Cannot bind to port %d",
			         protocol_to_str(p->type), p->port);
			return (UPERF_FAILURE);
		} else {
			if (p->port == ANY_PORT) {
				memset(&sin, 0, sizeof(struct sockaddr_in));
				len = (socklen_t)sizeof(struct sockaddr_in);
				if ((getsockname(p->fd, (struct sockaddr *)&sin, &len)) < 0) {
					ulog_err("%s: Cannot getsockname",
					         protocol_to_str(p->type));
					return (UPERF_FAILURE);
				} else {
					p->port = ntohs(sin.sin_port);
				}
			}
		}
	}
	listen(p->fd, LISTENQ);
	uperf_debug("Listening on port %d\n", p->port);

	return (p->port);
}

/*
 * On success, a non negative value is returned. 0 is returned to
 * indicate timeout. timeout is in millisecs
 */
int
generic_poll(int fd, int timeout, short poll_type)
{
	struct pollfd pfd[2];

	if (timeout <= 0)
		return (0);

	pfd[0].fd = fd;
	pfd[0].events = poll_type;
	pfd[0].revents = 0;

	return (poll(pfd, 1, timeout));
}

int
generic_disconnect(protocol_t *p)
{
	if (p && p->fd >= 0) {
		(void) close(p->fd);
		p->fd = -1;
	}

	return (UPERF_SUCCESS);
}

int
generic_read(protocol_t *p, void *buffer, int size, void *options)
{
	flowop_options_t *fo = (flowop_options_t *)options;
	int timeout = (fo ? fo->poll_timeout/1.0e+6 : 0);

	if (timeout > 0) {
		if ((generic_poll(p->fd, timeout, POLLIN)) <= 0)
			return (-1);
	}
	return (read(p->fd, buffer, size));
}

/* ARGSUSED */
int
generic_write(protocol_t *p, void *buffer, int size, void *options)
{
	return (write(p->fd, buffer, size));
}

/* ARGSUSED */
int
generic_recv(protocol_t *p, void *buffer, int size, void *options)
{
	return (recv(p->fd, buffer, size, 0));
}

/* ARGSUSED */
int
generic_send(protocol_t *p, void *buffer, int size, void *options)
{
	return (send(p->fd, buffer, size, 0));
}


/* ARGSUSED */
int
generic_undefined(protocol_t *p, void *options)
{
	uperf_error("Undefined function in protocol called\n");
	return (UPERF_FAILURE);
}

/* ARGSUSED2 */
int
generic_accept(protocol_t *oldp, protocol_t *newp, void *options)
{
	socklen_t addrlen;
	int timeout;
	char hostname[NI_MAXHOST];
	struct sockaddr_storage remote;

	assert(oldp);
	assert(newp);

	addrlen = (socklen_t)sizeof(struct sockaddr_storage);
	timeout = 10000;

	if ((generic_poll(oldp->fd, timeout, POLLIN)) <= 0)
		return (-1);

	if ((newp->fd = accept(oldp->fd, (struct sockaddr *)&remote,
	    &addrlen)) < 0) {
		ulog_err("accept:");
		return (UPERF_FAILURE);
	}
	switch (remote.ss_family) {
	case AF_INET:
	{
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)&remote;
		inet_ntop(AF_INET, &sin->sin_addr, hostname, sizeof(hostname));
		newp->port = ntohs(sin->sin_port);
		break;
	}
	case AF_INET6:
	{
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)&remote;
		inet_ntop(AF_INET6, &sin6->sin6_addr, hostname, sizeof(hostname));
		newp->port = ntohs(sin6->sin6_port);
		break;
	}
	default:
		return (UPERF_FAILURE);
		break;
	}
	(void) strlcpy(newp->host, hostname, sizeof(newp->host));
	uperf_info("Accepted connection from %s:%d\n", newp->host, newp->port);

#if 0
	if ((error = getnameinfo((const struct sockaddr *)&remote, addrlen,
	                         hostname, sizeof(hostname), NULL, 0, 0)) == 0) {
		if (remote.ss_family == AF_INET) {
			newp->port = ntohs(((struct sockaddr_in *)&remote)->sin_port);
		} else {
			newp->port = ntohs(((struct sockaddr_in6 *)&remote)->sin6_port);
		}
		(void) strlcpy(newp->host, hostname, MAXHOSTNAME);
		uperf_info("Accepted connection from %s:%d\n",
		           newp->host, newp->port);
	} else {
		char msg[128];
		(void) snprintf(msg, sizeof (msg),
		    "getnameinfo failed after accept: %s", gai_strerror(error));
		uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
		return (errno);
	}
#endif
	return (UPERF_SUCCESS);
}

void
generic_fini(protocol_t *p)
{
	if (p && p->fd > 0)
		(void) close(p->fd);
	if (p)
		free(p);
}

void
set_tcp_options(int fd, flowop_options_t *f)
{
	/*
	 * It is necessary that we set the send and recv buffer
	 * sizes *before* connect() and bind(), else it will
	 * not work
	 *
	 * We also need to make sure that we verify the buffer
	 * sizes after the connect and listen
	 */
	int wndsz;

	if (!f) {
		/*
		 * We used to se buffer size to 1M in the past
		 * But on GNU/linux if you do not set the window
		 * size, it autotunes.
		 */
		wndsz = 0;
	} else {
		wndsz = f->wndsz;
	}
	(void) generic_set_socket_buffer(fd, wndsz);
	(void) generic_verify_socket_buffer(fd, wndsz);

	if (f && (FO_TCP_NODELAY(f))) {
		int nodelay = 1;
		if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
			(char *)&nodelay, sizeof (nodelay))) {
			ulog_warn("Cannot set TCP_NODELAY:");
		}
	}
	if (f && strlen(f->cc) > 0) {
#ifdef TCP_CONGESTION
		if (setsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, f->cc, strlen(f->cc)) < 0) {
			ulog_warn("Cannot set TCP_CONGESTION:");
		}
#else
		uperf_warn("Configuring TCP CC not supported");
#endif
	}
	if (f && strlen(f->stack) > 0) {
#ifdef TCP_FUNCTION_BLK
		struct tcp_function_set stack;

		memset(&stack, 0, sizeof(struct tcp_function_set));
		strlcpy(stack.function_set_name, f->stack, TCP_FUNCTION_NAME_LEN_MAX);
		if (setsockopt(fd, IPPROTO_TCP, TCP_FUNCTION_BLK, &stack, sizeof(struct tcp_function_set)) < 0) {
			ulog_warn("Cannot set TCP_FUNCTION_BLK:");
		}
#else
		uperf_warn("Configuring TCP stack not supported");
#endif
	}
}
