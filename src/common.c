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
 * along with uperf.  If not, see http://www.gnu.org/licenses.
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_SIGNAL_H
#include <signal.h> /* For SIGUSR2 */
#endif /* HAVE_SIGNAL_H */

#ifdef HAVE_SYS_BYTEORDER_H
#include <sys/byteorder.h>
#endif /* HAVE_SYS_BYTEORDER_H */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h> /* for read(2) and write(2) */
#include <netinet/in.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#include "uperf.h"
#include "protocol.h"
#include "logging.h"
#include "flowops.h"
#include "workorder.h"
#include "stats.h"
#include "strand.h"
#include "shm.h"
#include "common.h"

int
update_strand_with_slave_info(uperf_shm_t *shm, slave_info_t *si,
    char * host, int nthr, int ssid)
{
	int i;

	uperf_debug("Updating slaveinfo for host %s\n", host);
	for (i = 0; i < nthr; i++) {
		strand_t *s = shm_get_strand(shm, i+ssid);
		assert(s);
		(void) strlcpy(si[i].host, host, MAXHOSTNAME);
		if ((strand_add_slave(s, &si[i])) != UPERF_SUCCESS) {
			free(si);
			return (UPERF_FAILURE);
		}
	}

	return (UPERF_SUCCESS);
}


static int
create_protocols(uperf_shm_t *shm, int nthr, flowop_t *f,
    slave_info_t *sl, int ssid)
{
	protocol_t *p;
	int i;
	int protocol = f->options.protocol;

	/* If already created, no need to repeat */
	if (sl[0].port[protocol] > 0)
		return (UPERF_SUCCESS);

	if (f->options.port != 0) {
		int port = htons(f->options.port);
		p = create_protocol(protocol, " ", ntohs(port), SLAVE);
		sl[0].port[protocol] = p->listen(p, (void *)&f->options);
		if (sl[0].port[protocol] <= 0) {
			return (UPERF_FAILURE);
		}
		for (i = 0; i < nthr; i++) {
			strand_t *s = shm_get_strand(shm, i + ssid);
			s->listen_conn[protocol] = p;
			sl[i].port[protocol] = sl[0].port[protocol];
		}
		return (UPERF_SUCCESS);
	}

	/* One port per thread */
	for (i = 0; i < nthr; i++) {
		strand_t *s = shm_get_strand(shm, i + ssid);
		p = create_protocol(protocol, " ", ANY_PORT, SLAVE);
		sl[i].port[protocol] = p->listen(p, (void *)&f->options);
		if (sl[i].port[protocol] <= 0) {
			return (UPERF_FAILURE);
		}
		s->listen_conn[protocol] = p;
	}
	return (UPERF_SUCCESS);
}


/*
 * Take a group_t and for all ACCEPT, precreate
 * listening ports. Care is taken to assign them to
 * threads with the right ID. It is the caller's
 * responsiblity to free sl on SUCCESS
 */

int
preprocess_accepts(uperf_shm_t *shm, group_t *g, slave_info_t **sl,
    int ssid)
{
	int status;
	txn_t *t;
	flowop_t *f;

	*sl = NULL;
	status = UPERF_SUCCESS;
	for (t = g->tlist; t && status == UPERF_SUCCESS; t = t->next) {
		for (f = t->flist; f; f = f->next) {
			if (status != UPERF_SUCCESS)
				break;
			if (f->type == FLOWOP_ACCEPT) {
				if (*sl == NULL) {
					*sl = calloc(1,
						sizeof (slave_info_t)
						* g->nthreads);
					if (*sl == NULL)
						return (UPERF_FAILURE);
				}
				status = create_protocols(shm, g->nthreads,
				    f, *sl, ssid);
				shm->tx_no_slave_info = g->nthreads;
			}
		}
	}

	if (status == UPERF_FAILURE) {
		if (sl != NULL)
			free (*sl);
		return (UPERF_FAILURE);
	}

	return (UPERF_SUCCESS);
}

static char cmds[][64] = { "UPERF_CMD_NEXT_TXN",
	"UPERF_CMD_ABORT",
	"UPERF_CMD_SEND_STATS",
	"UPERF_CMD_ERROR"};
int
uperf_get_command(protocol_t *p, uperf_command_t *uc, int bitswap)
{
	(void) memset(uc, 0, sizeof (uperf_command_t));
	int read_ret;
	read_ret = p->read(p, uc, sizeof (uperf_command_t), NULL);
	if (read_ret < 0) {
		(void) printf("Error during reading command: %s\n", strerror(errno));
		return (-1);
	} else if (read_ret == 0) {
		(void) printf("Error during reading command: connection closed\n");
		return (-1);
	}
	if (strncmp(uc->magic, UPERF_COMMAND_MAGIC, 64) != 0) {
		(void) printf("Wrong command magic\n");
		return (-1);
	}
	if (bitswap) {
		uc->command = BSWAP_32(uc->command);
		uc->value = BSWAP_32(uc->value);
	}
	uperf_info("RX Command [%s, %d] from %s\n", cmds[uc->command],
	    uc->value, p->host);
	return (0);
}

/* One command is sent at the end of each txn by the master */
int
uperf_send_command(protocol_t *p, uperf_cmd command, uint32_t val)
{
	uperf_command_t uc;

	(void) strlcpy(uc.magic, UPERF_COMMAND_MAGIC, 64);
	uc.command = command;
	uc.value = val;
	uperf_info("TX command [%s, %d] to %s\n", cmds[uc.command], uc.value,
	    p->host);

	return (p->write(p, &uc, sizeof (uperf_command_t), NULL));
}

int
ensure_read(protocol_t *p, void *buffer, int size)
{
	int n, sz;

	sz = 0;
	while (sz < size) {
		if ((n = read(p->fd, (char *)buffer + sz, size - sz)) <= 0) {
			return (n);
		}
		sz += n;
	}
	return (sz);
}

int
ensure_write(protocol_t *p, void *buffer, int size)
{
	int n, sz;

	sz = 0;
	while (sz < size) {
		if ((n = write(p->fd, (char *)buffer + sz, size - sz)) <= 0) {
			return (n);
		}
		sz += n;
	}
	return (sz);
}
