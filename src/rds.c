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
#endif /*  HAVE_SYS_POLL_H */

#include "uperf.h"
#include "protocol.h"
#include "logging.h"
#include "flowops.h"
#include "workorder.h"
#include "generic.h"

#define	RDS_HANDSHAKE	"uperf rds handshake"
/*
 * Private parameters for RDS
 */

typedef struct {
	int sock;	/* for doing the communication */
	char *rhost;
	int port;
	int refcount;
	struct sockaddr_in addr_info_from; /* Source of the RDS communication */
	struct sockaddr_in addr_info_to; /* destination */
} rds_private_data;


protocol_t *protocol_rds_create(char *host, int port);

static int
read_one(int fd, char *buffer, int len, struct sockaddr_in from)
{
	struct msghdr	msg;
	struct iovec	iov;
	int	ret;

	iov.iov_base = buffer;
	iov.iov_len = len;

	msg.msg_name = (caddr_t) &from;
	msg.msg_namelen = sizeof (from);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	msg.msg_control = NULL;
	msg.msg_controllen = 0;

	/* For the moment this will block for all receive */
	ret = recvmsg(fd, &msg, MSG_WAITALL);
	if (ret <= 0) {
		if (errno != EWOULDBLOCK)
			uperf_log_msg(UPERF_LOG_ERROR, errno, "recvfrom:");
		return (-1);
	}
	return (ret);
}

static int
write_one(int fd, char *buffer, int len, struct sockaddr_in to)
{
	/* Write a message after configuring the IOV buffers */
	struct msghdr   msg;
	struct iovec    iov;

	iov.iov_base = buffer;
	iov.iov_len = len;

	msg.msg_name = (caddr_t) &to;
	msg.msg_namelen = sizeof (to);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	return (sendmsg(fd, &msg, 0));
}

static int
protocol_rds_read(protocol_t *p, void *buffer, int n, void *options)
{
	rds_private_data *pd = (rds_private_data *) p->_protocol_p;

	int ret;
	int nleft;
	int timeout = 0;
	flowop_options_t *fo = (flowop_options_t *)options;

	if (fo != NULL) {
		timeout = (int) fo->poll_timeout/1.0e+6;
	}

	nleft = n;

	ret = read_one(pd->sock, buffer, n, pd->addr_info_from);

	if ((ret != n) && (errno != EWOULDBLOCK)) {
			uperf_log_msg(UPERF_LOG_ERROR, errno,
			    "non-block write");
			return (ret);
	}

	nleft = n - ret;

	if ((nleft > 0) && (timeout > 0)) {
		ret = generic_poll(pd->sock, timeout, POLLIN);
			if (ret > 0)
				return (read_one(pd->sock,
				buffer, nleft, pd->addr_info_from));
			else
				return (-1);
	} else if ((nleft > 0) && (timeout <= 0)) {
		/* Vanilla read */
		return (read_one(pd->sock, buffer, nleft,
			pd->addr_info_from));
	}
	return (ret);
}

/*
 * Function: protocol_rds_write: Implementation of RDS write
 */

static int
protocol_rds_write(protocol_t *p, void *buffer, int n, void *options)
{
	rds_private_data *pd = (rds_private_data *) p->_protocol_p;
	int ret;
	size_t nleft;
	int timeout = 0;
	flowop_options_t *fo = (flowop_options_t *)options;

	if (fo != NULL) {
		timeout = (int) fo->poll_timeout/1.0e+6;
	}

	nleft = n;
	ret = write_one(pd->sock, buffer, n,
			pd->addr_info_to);

	if ((ret <= 0) && (errno != EWOULDBLOCK) && (errno != ENOBUFS)) {
		uperf_log_msg(UPERF_LOG_ERROR, errno,
			"non-block write");
		return (-1);
	} else if (ret > 0) {
		nleft = n - ret;
	}

	if ((nleft > 0) && (timeout > 0)) {
		/* Need to poll */
		struct pollfd pfd[1];
		pfd[0].fd = pd->sock;
		pfd[0].events = POLLOUT;
		pfd[0].revents = 0;
		poll(pfd, 1, timeout);
		if (pfd[0].revents & POLLOUT) {
			write_one(pd->sock, buffer, nleft,
				pd->addr_info_to);
		}
		else
			return (-1);
	} else if ((nleft > 0) && (timeout <= 0)) {
		/* Vanilla write */
		return (write_one(pd->sock, buffer, nleft,
			pd->addr_info_to));
	}

	if (ret != n) {
		uperf_log_msg(UPERF_LOG_ERROR, errno,
			"very bad error encountered");
	}
	return (ret);
}

/*
 * Establish RDS channel on receiver side.
 */

static int
protocol_rds_listen(protocol_t *p, void *options)
{
	rds_private_data *pd = (rds_private_data *)p->_protocol_p;

	flowop_options_t *fo = (flowop_options_t *)options;
#ifdef UPERF_LINUX
	pd->sock = socket(PF_RDS, SOCK_SEQPACKET, 0);
#else
	/* Assumption that it is Solaris here and not NSD etc */
	pd->sock = socket(AF_INET_OFFLOAD, SOCK_DGRAM, 0);
#endif
	if (pd->sock < 0) {
		uperf_log_msg(UPERF_LOG_ERROR, errno, "socket");
		return (UPERF_FAILURE);
	}
	memset((char *) &pd->addr_info_from, 0, sizeof (pd->addr_info_from));
#ifdef UPERF_LINUX
	pd->addr_info_from.sin_family = AF_INET;
#else
	pd->addr_info_from.sin_family = AF_INET_OFFLOAD;
#endif
	/* Note: This works on the profile sent from master only. */

	pd->addr_info_from.sin_addr.s_addr = inet_addr(fo->localhost);
	pd->addr_info_from.sin_port = htons(pd->port);

	if ((bind(pd->sock, (struct sockaddr *)&pd->addr_info_from,
			sizeof (pd->addr_info_from))) < 0) {
		uperf_log_msg(UPERF_LOG_ERROR, errno, "bind");
		return (UPERF_FAILURE);
	}
	if (pd->addr_info_from.sin_port == ANY_PORT) {
		int tmp = sizeof (pd->addr_info_from);
		if ((getsockname(pd->sock,
		(struct sockaddr *)&pd->addr_info_from, &tmp)) != 0) {
			uperf_log_msg(UPERF_LOG_ERROR, errno, "getsockname");
			return (UPERF_FAILURE);
		}
	}
	pd->port = ntohs(pd->addr_info_from.sin_port);

	/* Now connect to the masters IP address */

	memset((char *) &pd->addr_info_to, 0, sizeof (pd->addr_info_to));

#ifdef UPERF_LINUX
	pd->addr_info_to.sin_family = AF_INET;
#else
	pd->addr_info_to.sin_family = AF_INET_OFFLOAD;
#endif
	pd->addr_info_to.sin_addr.s_addr = inet_addr(fo->remotehost);
	pd->addr_info_to.sin_port = htons(pd->port);
	uperf_debug("listening on port %s:%d\n",
	inet_ntoa(pd->addr_info_from.sin_addr), pd->addr_info_from.sin_port);
	return (pd->port);
}

static protocol_t *
protocol_rds_accept(protocol_t *p, void *options)
{
	char msg[32];
	rds_private_data *pd = (rds_private_data *)p->_protocol_p;

	(void) bzero(msg, sizeof (msg));
	if ((protocol_rds_read(p, msg, strlen(RDS_HANDSHAKE), NULL) <=
		UPERF_SUCCESS)) {
		uperf_log_msg(UPERF_LOG_ERROR, errno,
		"Error in UDP Handshake");
		uperf_info("\nError in UDP Handshake\n");
		return (NULL);
	}
	if (strcmp(msg, RDS_HANDSHAKE) != 0)
		return (NULL);
	pd->refcount++;

	uperf_info("Handshake[%s] with %s:%d\n", msg,
	    inet_ntoa(pd->addr_info_from.sin_addr),
		pd->addr_info_from.sin_port);

	return (p);
}

static int
protocol_rds_connect(protocol_t *p, void *options)
{
	rds_private_data *pd = (rds_private_data *)p->_protocol_p;
	flowop_options_t *fo = (flowop_options_t *)options;

#ifdef UPERF_LINUX
	pd->sock = socket(PF_RDS, SOCK_SEQPACKET, 0);
#else
	pd->sock = socket(AF_INET_OFFLOAD, SOCK_DGRAM, 0);
#endif
	if (pd->sock < 0) {
		uperf_fatal("RDS - Error opening socket");
	}

	/* Bind the socket */
#ifdef UPERF_LINUX
	pd->addr_info_from.sin_family = AF_INET;
#else
	pd->addr_info_from.sin_family = AF_INET_OFFLOAD;
#endif
	pd->addr_info_from.sin_addr.s_addr
		= inet_addr(fo->localhost);
	pd->addr_info_from.sin_port = htons(pd->port);

	/* Bind */

	if ((bind(pd->sock, (struct sockaddr *)&pd->addr_info_from,
		sizeof (pd->addr_info_from))) < 0) {
		uperf_log_msg(UPERF_LOG_ERROR, errno,
			"Error in RDS Bind Operation");
		uperf_info("\nerror in RDS Bind Operation");
		return (UPERF_FAILURE);
	}

	memset((char *) &pd->addr_info_to, 0, sizeof (pd->addr_info_to));
#ifdef UPERF_LINUX
	pd->addr_info_to.sin_family = AF_INET;
#else
	pd->addr_info_to.sin_family = AF_INET_OFFLOAD;
#endif
	pd->addr_info_to.sin_addr.s_addr
		= inet_addr(fo->remotehost);
	pd->addr_info_to.sin_port = htons(pd->port);

	if ((protocol_rds_write(p, RDS_HANDSHAKE, strlen(RDS_HANDSHAKE),
		    NULL)) <= 0) {
		uperf_log_msg(UPERF_LOG_ERROR, errno, "Error in RDS Handshake");
		uperf_info("\nError in RDS Handshake\n");
		return (UPERF_FAILURE);
	}
	return (UPERF_SUCCESS);
}

static int
protocol_rds_disconnect(protocol_t *p)
{
	rds_private_data *pd = (rds_private_data *) p->_protocol_p;
	uperf_debug("rds - disconnect done\n");
	pd->refcount--;
	return (UPERF_SUCCESS);
}

protocol_t *
protocol_rds_create(char *host, int port)
{
	protocol_t *newp;
	rds_private_data *new_rds_p;

	if ((newp = calloc(1, sizeof(protocol_t))) == NULL) {
		perror("calloc");
		return (NULL);
	}
	/* Allocating a local data structure */
	if ((new_rds_p = calloc(1, sizeof(rds_private_data))) == NULL) {
		perror("calloc");
		return (NULL);
	}
	newp->connect = &protocol_rds_connect;
	newp->disconnect = &protocol_rds_disconnect;
	newp->read = &protocol_rds_read;
	newp->write = &protocol_rds_write;
	newp->listen = &protocol_rds_listen;
	newp->accept = &protocol_rds_accept;
	newp->wait = &generic_undefined;
	if (strlen(host) == 0) {
		(void) strlcpy(newp->host, "localhost", MAXHOSTNAME);
	} else {
		(void) strlcpy(newp->host, host, MAXHOSTNAME);
	}
	newp->port = port;
	newp->type = PROTOCOL_RDS;
	newp->_protocol_p = new_rds_p;
	new_rds_p->refcount = 0;
	uperf_debug("rds - Creating RDS Protocol to %s:%d\n", host, port);
	return (newp);
}

void
rds_fini(protocol_t *p)
{
	rds_private_data *pd;
	static int called = 0;
	called++;
	if (!p)
		return;
	pd = (rds_private_data *) p->_protocol_p;
	if (!pd)
		return;
	/*
	 * In RDS, we use only one socket to communicate. If
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
