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

int
name_to_addr(const char *address, struct in_addr *saddr)
{
	char buf[1024];
	int  error;
	struct hostent *host, res;

	/* First try it as aaa.bbb.ccc.ddd. */
	saddr->s_addr = inet_addr(address);
	if (saddr->s_addr != (in_addr_t)-1) {
		return (0);
	}
#ifndef UPERF_SOLARIS
	if (0 == (gethostbyname_r(address, (void *)&res, buf,
		    sizeof (buf), &host,  &error))) {
#else
	if ((host = gethostbyname_r(address, (void *)&res, buf,
			sizeof (buf), &error))) {
#endif /* UPERF_LINUX */
		(void) memcpy((char *)saddr, host->h_addr_list[0],
		    host->h_length);
		return (0);
	}
	return (-1);
}
int
generic_socket(protocol_t *p, int pflag)
{
	char msg[128];
	const char *pname = protocol_to_str(p->type);

	if ((p->fd = socket(AF_INET, SOCK_STREAM, pflag)) < 0) {
		(void) snprintf(msg, 128, "%s: Cannot create socket",
		    pname);
		uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
		return (-1);
	}
	return (0);
}

/* ARGSUSED */
int
generic_connect(protocol_t *p, int pflag)
{
	struct sockaddr_in server;
	const char *pname;
	char msg[128];

	uperf_debug("Connecting to %s:%d\n", p->host, p->port);

	if ((generic_socket(p, pflag)) < 0)
		return (-1);

	(void) memset(&server, 0, sizeof (server));
	if (name_to_addr(p->host, &(server.sin_addr))) {
		(void) snprintf(msg, sizeof (msg),
				"Cannot connect to Unknown host: %s",
				p->host);
		uperf_log_msg(UPERF_LOG_ERROR, 0, msg);
		return (-1);
	}
	pname = protocol_to_str(p->type);

	server.sin_port = htons(p->port);
	server.sin_family = AF_INET;

	if ((connect(p->fd, (struct sockaddr *)&server,
		    sizeof (server))) < 0) {
		(void) snprintf(msg, 128, "%s: Cannot connect to %s:%d",
		    pname,
		    p->host, p->port);
		uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
		uperf_info(msg);
		return (-1);
	}

	return (0);
}
int
generic_setfd_nonblock(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		uperf_log_msg(UPERF_LOG_WARN, errno,
			"Cannot set non-blocking, falling back to default");
		return (-1);
	}

	return (0);
}

int
generic_set_socket_buffer(int fd, int size)
{
	int wndsz = size;

	if (wndsz == 0)
		return (0);
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&wndsz,
			sizeof (wndsz))) {
		uperf_log_msg(UPERF_LOG_WARN, errno, "Cannot set SO_SNDBUF");
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&wndsz,
			sizeof (wndsz)) != 0) {
		uperf_log_msg(UPERF_LOG_WARN, errno, " Cannot set SO_RCVBUF");
	}

	return (UPERF_SUCCESS);
}

int
generic_verify_socket_buffer(int fd, int wndsz)
{
	int nwndsz;
	int len;
	float diff;
	char msg[128];

	if (wndsz == 0)
		return (0);

	/* Now verify */
	len = sizeof (wndsz);
	nwndsz = -1;
	if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&nwndsz,
		&len) != 0) {
		uperf_log_msg(UPERF_LOG_WARN, errno, "Cannot get SO_SNDBUF");
	}
	diff = 1.0*nwndsz/wndsz;
	if (diff < 0.9 || diff > 1.1) {
		(void) snprintf(msg, 128, "%s: %.2fKB (Requested:%.2fKB)",
		    "SNDBUF", nwndsz/1024.0, wndsz/1024.0);
		uperf_log_msg(UPERF_LOG_WARN, 0, msg);
	} else {
		uperf_info("Set Send buffer size to %.2fKB\n", nwndsz/1024.0);
	}

	len = sizeof (wndsz);
	if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&nwndsz,
		&len) != 0) {
		uperf_log_msg(UPERF_LOG_WARN, errno, "Cannot get SO_RCVBUF");
	}

	diff = 1.0*nwndsz/wndsz;
	if (diff < 0.9 || diff > 1.1) {
		(void) snprintf(msg, 128, "%s: %.2fKB (Requested:%.2fKB)",
		    "Recv buffer", nwndsz/1024.0, wndsz/1024.0);
		uperf_log_msg(UPERF_LOG_WARN, 0, msg);
	} else {
		uperf_info("Set Recv buffer size to %.2fKB\n", nwndsz/1024.0);
	}

	return (0);
}

/* ARGSUSED */
int
generic_listen(protocol_t *p, int pflag)
{
	int reuse = 1;
	struct sockaddr_in server;
	const char *pname;
	char msg[128];

	pname = protocol_to_str(p->type);

	if (setsockopt(p->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse,
		sizeof (int)) < 0) {
		(void) snprintf(msg, 128, "%s: Cannot set SO_REUSEADDR",
		    pname);
		uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
	}
	(void) memset(&server, 0, sizeof (struct sockaddr_in));
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_family = AF_INET;
	server.sin_port = htons(p->port);
	if ((bind(p->fd, (struct sockaddr *)&server,
		    sizeof (server))) != 0) {
		(void) snprintf(msg, 128, "%s: Cannot bind to port", pname);
		uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
		return (-1);
	}
	if (server.sin_port == ANY_PORT) {
		int tmp = sizeof (server);
		if ((getsockname(p->fd, (struct sockaddr *)&server,
			    &tmp)) != 0) {
			(void) snprintf(msg, 128, "%s: Cannot getsockname",
			    pname);
			uperf_log_msg(UPERF_LOG_ERROR, errno, msg);
			return (-1);
		}
	}
	listen(p->fd, LISTENQ);
	p->port = ntohs(server.sin_port);
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
	int addrlen;
	int error;
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
		uperf_log_msg(UPERF_LOG_ERROR, errno, "accept:");
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
			uperf_log_msg(UPERF_LOG_WARN, errno,
			    "Cannot set TCP_NODELAY");
		}
	}
}
