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
	struct sockaddr_in addr_info;
}udp_private_data;

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
		if (generic_setfd_nonblock(fd) != 0)
			CLEAR_FO_NONBLOCKING(f);
	}

	return (0);
}

static int
read_one(int fd, char *buffer, int len, struct sockaddr *from)
{
	int ret;
	int length = (socklen_t)sizeof (*from);

	/*
	 * uperf_debug("recvfrom %s:%d\n", inet_ntoa(saddr->sin_addr),
	 * saddr->sin_port);
	 */

	ret = recvfrom(fd, buffer, len, 0, from, &length);
	if (ret <= 0) {
		if (errno != EWOULDBLOCK)
			uperf_log_msg(UPERF_LOG_ERROR, errno, "recvfrom:");
		return (-1);
	}

	return (ret);
}
static int
write_one(int fd, char *buffer, int len, struct sockaddr *from)
{
	int length = (socklen_t)sizeof (*from);

	return (sendto(fd, buffer, len, 0, from, length));
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
		ret = read_one(pd->sock, buffer, n,
			(struct sockaddr *)&pd->addr_info);
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
			return (read_one(pd->sock, buffer, nleft,
				(struct sockaddr *)&pd->addr_info));
		else
			return (-1); /* ret == 0 means timeout (error); */
	} else if ((nleft > 0) && (timeout <= 0)) {
		/* Vanilla read */
		return (read_one(pd->sock, buffer, nleft,
			(struct sockaddr *)&pd->addr_info));
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

	pd->sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (pd->sock < 0) {
		uperf_log_msg(UPERF_LOG_ERROR, errno, "socket");
		return (UPERF_FAILURE);
	}

	(void) bzero((char *)&pd->addr_info, sizeof (struct sockaddr_in));
	pd->addr_info.sin_family = AF_INET;
	pd->addr_info.sin_addr.s_addr = htonl(INADDR_ANY);
	pd->addr_info.sin_port = htons(pd->port);
	if ((bind(pd->sock, (struct sockaddr *)&pd->addr_info,
		    sizeof (pd->addr_info))) < 0) {
		uperf_log_msg(UPERF_LOG_ERROR, errno, "bind");
		return (UPERF_FAILURE);
	}
	if (pd->addr_info.sin_port == ANY_PORT) {
		int tmp = sizeof (pd->addr_info);
		if ((getsockname(pd->sock, (struct sockaddr *)&pd->addr_info,
			    &tmp)) != 0) {
			uperf_log_msg(UPERF_LOG_ERROR, errno, "getsockname");
			return (UPERF_FAILURE);
		}
	}
	pd->port = ntohs(pd->addr_info.sin_port);

	uperf_debug("listening on port %s:%d\n",
	    inet_ntoa(pd->addr_info.sin_addr), pd->addr_info.sin_port);

	return (pd->port);
}

static protocol_t *
protocol_udp_accept(protocol_t *p, void *options)
{
	char msg[32];
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

	uperf_info("Handshake[%s] with %s:%d\n", msg,
	    inet_ntoa(pd->addr_info.sin_addr), pd->addr_info.sin_port);

	return (p);
}

static int
protocol_udp_connect(protocol_t *p, void *options)
{
	udp_private_data *pd = (udp_private_data *)p->_protocol_p;
	flowop_options_t *fo = (flowop_options_t *)options;

	(void) bzero(&pd->addr_info, sizeof (struct sockaddr_in));
	if (name_to_addr(pd->rhost, &(pd->addr_info)) != 0) {
		perror("name_to_addr:");
		uperf_fatal("Unknown host: %s\n", pd->rhost);
	}
	pd->addr_info.sin_port = htons(pd->port);
	pd->addr_info.sin_family = AF_INET;
	pd->sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (pd->sock < 0) {
		uperf_fatal("udp - Error opening socket");
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
	uperf_debug("udp - disconnect done");

	pd->refcount--;

	return (UPERF_SUCCESS);
}

protocol_t *
protocol_udp_create(char *host, int port)
{
	protocol_t *p;
	udp_private_data *d;

	p = calloc(1, sizeof (protocol_t));
	d = calloc(1, sizeof (udp_private_data));
	p->connect = &protocol_udp_connect;
	p->disconnect = &protocol_udp_disconnect;
	p->read = &protocol_udp_read;
	p->write = &protocol_udp_write;
	p->listen = &protocol_udp_listen;
	p->accept = &protocol_udp_accept;
	p->wait = &generic_undefined;
	d->rhost = strdup(host);
	d->port = port;
	p->type = PROTOCOL_UDP;
	p->_protocol_p = d;
	d->refcount = 0;
	uperf_debug("udp - Creating UDP Protocol to %s:%d", host, port);
	return (p);
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
