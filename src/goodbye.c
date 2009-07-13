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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <unistd.h>
#ifdef UPERF_LINUX
#include <sys/poll.h>
#else
#include <poll.h>
#endif /* UPERF_LINUX */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include "uperf.h"
#include "protocol.h"
#include "generic.h"
#include "sync.h"
#include "logging.h"
#include "flowops.h"
#include "workorder.h"
#include "stats.h"
#include "strand.h"
#include "shm.h"
#include "goodbye.h"

/* Guaranteed to return within timeout msecs. returns 0 on success */
static int
safe_read(int fd, char *buffer, int size, int timeout)
{
	int ret;
	int index;
	int remain;
	double stop = GETHRTIME() + timeout * 1.0e+6;

	index = 0;
	remain = size;

	if ((ret = generic_setfd_nonblock(fd)) != 0) {
		perror("goodbye:");
		return (ret);
	}
	while ((remain > 0) && (GETHRTIME() < stop)) {
		if ((ret = generic_poll(fd, 3000, POLLIN)) < 0)
			return (ret);
		else if (ret == 0) {
			errno = ETIMEDOUT;
			return (-1);
		}

		/* Try to read, might return EAGAIN */
		if ((ret = read(fd, &buffer[index], remain)) <= 0) {
			if (errno == EAGAIN)
				continue;
			else
				return (ret);
		}
		index += ret;
		remain -= ret;
	}
	if (remain == 0)
		return (0);
	return (-1);
}
int
bitswap_goodbye_t(goodbye_t *g)
{
	g->msg_type = BSWAP_64(g->msg_type);
	g->gstat.elapsed_time = BSWAP_64(g->gstat.elapsed_time);
	g->gstat.error = BSWAP_64(g->gstat.error);
	g->gstat.bytes_xfer = BSWAP_64(g->gstat.bytes_xfer);
	g->gstat.count = BSWAP_64(g->gstat.count);

	return (0);
}
int
send_goodbye(goodbye_t *g, protocol_t *p)
{
	assert(p);
	assert(g);
	(void) strlcpy(g->magic, GOODBYE_MAGIC, sizeof (g->magic));
	if (p->write(p, g, sizeof (goodbye_t), NULL) <= 0) {
		uperf_info("Error exchanging goodbye's with client ");
		return (UPERF_FAILURE);
	}
	return (UPERF_SUCCESS);

}
int
recv_goodbye(goodbye_t *g, protocol_t *p, int timeout)
{
	assert(p);
	assert(g);

	bzero(g, sizeof(goodbye_t));
	if (safe_read(p->fd, (char *)g, sizeof (goodbye_t), timeout)
	    != UPERF_SUCCESS) {
		uperf_info("Error exchanging goodbye's with client ");
		return (UPERF_FAILURE);
	}
	if (strncmp(g->magic, GOODBYE_MAGIC, sizeof (g->magic)) != 0) {
		(void) printf("Wrong goodbye magic: %s\n", g->magic);
		return (-1);
	}
	return (UPERF_SUCCESS);
}
