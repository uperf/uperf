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
#define	SOCK_PORT(sin)	((sin).sin_port)

extern int ensure_write(protocol_t *p, void *buffer, int size, void *options);
extern int ensure_read(protocol_t *p, void *buffer, int size, void *options);

int
name_to_addr(const char *address, struct sockaddr_in *saddr)
{
	struct addrinfo *res;
	int error = 0;
	if ((error = getaddrinfo(address, NULL, NULL, &res)) == 0) {
		memcpy(saddr, res->ai_addr, sizeof (struct sockaddr_in));
		freeaddrinfo(res);
	} else {
		ulog_err("getaddrinfo(%s): %s\n", address, gai_strerror(error));
        }
	return (error);
}

int
generic_socket(protocol_t *p, int pflag)
{
	if ((p->fd = socket(AF_INET, SOCK_STREAM, pflag)) < 0) {
		ulog_err("%s: Cannot create socket", protocol_to_str(p->type));
		return (-1);
	}
	return (0);
}

/* ARGSUSED */
int
generic_connect(protocol_t *p, int pflag)
{
	struct sockaddr_in serv;

	uperf_debug("Connecting to %s:%d\n", p->host, p->port);

	if ((generic_socket(p, pflag)) < 0)
		return (-1);

	(void) memset(&serv, 0, sizeof (serv));
	if (name_to_addr(p->host, &serv)) {
		/* Error is already reported by name_to_addr, so just return */
		return (-1);
	}

	serv.sin_port = htons(p->port);
	serv.sin_family = AF_INET;

	if ((connect(p->fd, (struct sockaddr *)&serv, sizeof (serv))) < 0) {
		ulog_err("%s: Cannot connect to %s:%d",
			protocol_to_str(p->type), p->host, p->port);
		return (-1);
	}

	return (0);
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
		return (0);
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&w, sizeof (w))) {
		ulog_warn("Cannot set SO_SNDBUF");
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&w, sizeof (w)) !=0) {
		ulog_warn(" Cannot set SO_RCVBUF");
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
		return (0);

	/* Now verify */
	len = sizeof (wndsz);
	nwsz = -1;
	if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&nwsz, &len) != 0) {
		ulog_warn("Cannot get SO_SNDBUF");
	}
	diff = 1.0*nwsz/wndsz;
	if (diff < 0.9 || diff > 1.1) {
		ulog_warn("%s: %.2fKB (Requested:%.2fKB)", "SNDBUF",	
			nwsz/1024.0, wndsz/1024.0);
	} else {
		uperf_info("Set Send buffer size to %.2fKB\n", nwsz/1024.0);
	}

	len = sizeof (wndsz);
	if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&nwsz, &len) != 0) {
		ulog_warn("Cannot get SO_RCVBUF");
	}

	diff = 1.0*nwsz/wndsz;
	if (diff < 0.9 || diff > 1.1) {
		ulog_warn("%s: %.2fKB (Requested:%.2fKB)",
		    "Recv buffer", nwsz/1024.0, wndsz/1024.0);
	} else {
		uperf_info("Set Recv buffer size to %.2fKB\n", nwsz/1024.0);
	}

	return (0);
}

/* ARGSUSED */
int
generic_listen(protocol_t *p, int pflag)
{
	int reuse = 1;
	struct sockaddr_in serv;

	if (setsockopt(p->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse,
		sizeof (int)) < 0) {
		ulog_err("%s: Cannot set SO_REUSEADDR",
			protocol_to_str(p->type));
	}
	(void) memset(&serv, 0, sizeof (struct sockaddr_in));
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	serv.sin_family = AF_INET;
	serv.sin_port = htons(p->port);
	if ((bind(p->fd, (struct sockaddr *)&serv, sizeof (serv))) != 0) {
		ulog_err("%s: Cannot bind to port %d",
			protocol_to_str(p->type), serv.sin_port);
		return (-1);
	}
	if (serv.sin_port == ANY_PORT) {
		socklen_t tmp = sizeof (serv);
		if ((getsockname(p->fd, (struct sockaddr *)&serv, &tmp)) != 0) {
			ulog_err("%s: Cannot getsockname",
				protocol_to_str(p->type));
			return (-1);
		}
	}
	listen(p->fd, LISTENQ);
	p->port = ntohs(serv.sin_port);
	uperf_debug("Listening on port %d\n",  p->port);

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

int
generic_recv(protocol_t *p, void *buffer, int size, void *options)
{
	return (recv(p->fd, buffer, size, 0));
}

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
	struct sockaddr_in remote;

	assert(oldp);
	assert(newp);

	addrlen = sizeof (remote);
	timeout = 10000;

	if ((generic_poll(oldp->fd, timeout, POLLIN)) <= 0)
		return (-1);

	if ((newp->fd = accept(oldp->fd, (struct sockaddr *)&remote,
	    &addrlen)) < 0) {
		ulog_err("accept:");
		return (-1);
	}

	inet_ntop(remote.sin_family, &remote.sin_addr, hostname,
	    sizeof (hostname));
	(void) strlcpy(newp->host, hostname, sizeof (newp->host));
	newp->port = SOCK_PORT(remote);

	uperf_info("accept connection from %s\n", hostname);

#if 0
	if ((error = getnameinfo((const struct sockaddr *) &remote, addrlen,
	    hostname, sizeof (hostname), NULL, 0, 0)) == 0) {
		uperf_info("Accpeted connection from %s:%d\n", hostname,
		    SOCK_PORT(remote));
		(void) strlcpy(newp->host, hostname, MAXHOSTNAME);
		newp->port = SOCK_PORT(remote);
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
			ulog_warn("Cannot set TCP_NODELAY");
		}
	}
}
