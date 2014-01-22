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
 * along with uperf.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <assert.h>
#include <stddef.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_SYS_BYTEORDER_H
#define	__EXTENSIONS__
#include <sys/byteorder.h>
#endif /* HAVE_SYS_BYTEORDER_H */

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
#include "handshake.h"
#include "common.h"
#include "flowops_library.h"

#define	ENABLED_HANDSHAKE	1

/*
 * Transfer the group_t to a slave.
 * Before we transmit, we need to convert the master's work to what
 * the slave should do, and then transfer.
 */
static int
tx_group_t(protocol_t *p, group_t *g, char *host)
{
	txn_t *txn;
	flowop_t *fptr;
	group_t *copy;

	copy = group_clone(g);
	assert(copy);

	if ((group_opposite(copy)) != UPERF_SUCCESS) {
		return (UPERF_FAILURE);
	}
	uperf_info("  Sending workorder\n");
	/* Nop flowops not for this slave */
	for (txn = copy->tlist; txn; txn = txn->next) {
		for (fptr = txn->flist; fptr; fptr = fptr->next) {
			flowop_t *f1 = fptr;
			if (f1->options.remotehost[0] == '\0')
				continue;
			if (strcmp(host, f1->options.remotehost) != 0) {
				f1->type = FLOWOP_NOP;
				uperf_info("master - nop'ng for id:");
			}
		}
	}


	/* First write the group, then txn , then flowop */
	if (ensure_write(p, copy, SIZEOF_GROUP_T) <= 0) {
		group_free(copy);
		return (UPERF_FAILURE);
	}
	uperf_info("    Sent workorder\n");
	for (txn = copy->tlist; txn; txn = txn->next) {
		if (ensure_write(p, txn, SIZEOF_TXN_T) <= 0) {
			group_free(copy);
			return (UPERF_FAILURE);
		}
		uperf_info("    Sent transaction\n");
		for (fptr = txn->flist; fptr; fptr = fptr->next) {
			if (ensure_write(p, fptr, SIZEOF_FLOWOP_T) <= 0) {
				group_free(copy);
				return (UPERF_FAILURE);
			}
			uperf_info("    Sent flowop\n");
		}
	}
	group_free(copy);
	uperf_info("TX worklist success");

	return (UPERF_SUCCESS);
}

/*
 * Receive a group_t structure from the master.
 * Do byteswapping if necessary
 * Returns : group_t * on success, else NULL
 */
static group_t *
rx_group_t(protocol_t *p, int my_endian)
{
	int i, j;
	int bitswap = 0;
	group_t *worklist;
	txn_t *txn, *tptr;
	flowop_t *fptr;
	char temp[MAXHOSTNAME];

	tptr = NULL;

	uperf_info("Waiting for workorder\n");
	worklist = calloc(1, sizeof (group_t));
	if (ensure_read(p, worklist, SIZEOF_GROUP_T) <= 0) {
		free(worklist);
		return (NULL);
	}
	uperf_info("  Received workgroup\n")
	if (my_endian != worklist->endian) {
		uperf_info("Master has a different endian type!\n");
		worklist->ntxn = BSWAP_32(worklist->ntxn);
		bitswap = 1;
	}
	for (i = 0; i < worklist->ntxn; i++) {
		txn = calloc(1, sizeof (txn_t));
		if (ensure_read(p, txn, SIZEOF_TXN_T) <= 0) {
			group_free(worklist);
			free(txn);
			return (NULL);
		}
		uperf_info("  Received Transaction\n");

		fptr = NULL;
		if (bitswap == 1) {
			txn->nflowop = BSWAP_32(txn->nflowop);
		}
		uperf_info("  Transaction has %d flowops\n", txn->nflowop);
		for (j = 0; j <  txn->nflowop; j++) {
			flowop_t *f = calloc(1, sizeof (flowop_t));
			uperf_info("    Reading Flowop\n");
			if (ensure_read(p, f, SIZEOF_FLOWOP_T) <= 0) {
				group_free(worklist);
				free(f);
				return (NULL);
			}
			/* If localhost is present, swap */
			if (f->options.localhost[0] != '\0') {
				/* Swap remotehost and localhost */
				(void) strlcpy(temp,
					f->options.remotehost, MAXHOSTNAME);
				(void) strlcpy(f->options.remotehost,
					f->options.localhost, MAXHOSTNAME);
				(void) strlcpy(f->options.localhost, temp, MAXHOSTNAME);
			}
			else
			{
				(void) strlcpy(f->options.remotehost,
						p->host, MAXHOSTNAME);
			}
			if (bitswap == 1)
				f->type = BSWAP_32(f->type);
			f->execute = flowop_get_execute_func(f->type);
			if (fptr == NULL) {
				fptr = f;
				txn->flist = f;
			} else {
				fptr->next = f;
				fptr = f;
			}
		}
		if (tptr == NULL) {
			tptr = txn;
			worklist->tlist = tptr;
		} else {
			tptr->next = txn;
			tptr = txn;
		}
	}

	uperf_info("  Slave received worklist\n");

	return (worklist);
}

/*
 * connect to slaves *
 * 1. for all groups
 * 2. for all transactions
 * 3. for all flowops
 * 4. if FLOWOP_CONNECT || FLOWOP_ACCEPT
 * 5. already connected? continue
 * 6. handshake_start
 * 7. transfer_workoder
 * 8. get_ports
 * 9. update uperf_shm_t with port numbers
 * 10.handshake_end
 * 11 done
 */

static int
handshake_p1_with_slave(protocol_t *p, int *protocols)
{
	hs_phase1_t hsp1;
	hs_phase1_ack_t hsp1a;

	(void) memset(&hsp1, 0, sizeof (hs_phase1_t));
	(void) memset(&hsp1a, 0, sizeof (hs_phase1_ack_t));

	hsp1.phase = HANDSHAKE_PHASE_1;
	(void) strlcpy(hsp1.magic, UPERF_MAGIC, sizeof (hsp1.magic));
	hsp1.endian = UPERF_ENDIAN_VALUE; /* shm->endian; */
	(void) memcpy(hsp1.protocol, protocols, sizeof (hsp1.protocol));
	(void) strlcpy(hsp1.version, UPERF_DATA_VERSION, UPERF_VERSION_LEN);

	if ((ensure_write(p, &hsp1, sizeof (hs_phase1_t))) <= 0) {
		return (UPERF_FAILURE);
	}
	if ((ensure_read(p, &hsp1a, sizeof (hs_phase1_ack_t))) <= 0) {
		return (UPERF_FAILURE);
	}
	if (strcmp(hsp1a.status, UPERF_OK) != 0) {
		uperf_error("Error while handshaking with %s\n", p->host);
		uperf_error("%s says: %s\n", p->host, hsp1a.message);
		return (UPERF_FAILURE);
	}

	return (0);
}
/*
 * Handshake phase 1. Connect to all remote hosts
 * and verify magic, and version numbers, protocols
 * supported etc..
 */
static int
handshake_phase1(workorder_t *w)
{
	int i;
	group_t *g;

	for (i = 0; i < w->ngrp; i++) {
		g = &w->grp[i];
		protocol_t *p = g->control;
		while (p) {
			if (handshake_p1_with_slave(p, g->protocols) != 0) {
				uperf_error("error handshaking with %s\n",
				    p->host);
				return (UPERF_FAILURE);
			}
			p = p->next;
		}
	}

	return (UPERF_SUCCESS);
}

/*
 * Transfer workorder to the slave, wait for slave to
 * spin off threads and reply back with slave_info_t[]
 */
static int
handshake_p2_with_slave(uperf_shm_t *shm, char *host, group_t *g,
    int ssid, protocol_t *p)
{
	hs_phase2_t hsp2;
	hs_phase2_ack_t hsp2a;
	strand_t *strand;
	slave_info_t *si = NULL;
	int size;

	strand = shm_get_strand(shm, ssid);
	assert(strand);

	(void) memset(&hsp2, 0, sizeof (hs_phase2_t));
	(void) memset(&hsp2a, 0, sizeof (hs_phase2_ack_t));

	/* First preprocess any accepts before starting phase2 */
	if (preprocess_accepts(shm, g, &si, ssid) != UPERF_SUCCESS) {
		uperf_error("Error creating ports on the master side\n");
		return (UPERF_FAILURE);
	}
	uperf_info("  Done preprocessing accepts\n");

	hsp2.p1.phase = HANDSHAKE_PHASE_2;
	(void) strlcpy(hsp2.p1.magic, UPERF_MAGIC, sizeof (hsp2.p1.magic));
	hsp2.p1.endian = UPERF_ENDIAN_VALUE; /* shm->endian; */
	(void) strlcpy(hsp2.p1.version, UPERF_DATA_VERSION, UPERF_VERSION_LEN);

	if (si != NULL) {
		hsp2.p1.no_slave_info = g->nthreads;
	} else {
		hsp2.p1.no_slave_info = 0;
	}

	if ((ensure_write(p, &hsp2, sizeof (hs_phase2_t))) <= 0) {
		return (UPERF_FAILURE);
	}
	uperf_info("  Sent handshake header\n");

	if ((tx_group_t(p, g, host)) != UPERF_SUCCESS) {
		return (UPERF_FAILURE);
	}
	uperf_info("  Sent workorder\n");

	if (si != NULL) {
		/*
		 * Master threads are doing accepts, need to send that
		 * info the the slaves
		 */
		uperf_info("Sending %d slave_info_t\n", hsp2.p1.no_slave_info);
		if ((ensure_write(p, si, g->nthreads * sizeof (slave_info_t)))
			<= 0) {
			return (UPERF_FAILURE);
		}
		free(si);
	}

	if ((ensure_read(p, &hsp2a, sizeof (hs_phase2_ack_t))) <= 0) {
		return (UPERF_FAILURE);
	}

	if (strcmp(hsp2a.p1a.status, UPERF_OK) != 0) {
		uperf_error("Error while handshaking phase 2 with %s\n", host);
		uperf_error("%s says: %s\n", host, hsp2a.p1a.message);
		return (UPERF_FAILURE);
	}
	if (hsp2a.no_slave_info > 0) {
		size = hsp2a.no_slave_info * sizeof (slave_info_t);
		si = malloc(size);

		if (si == NULL) {
			perror("malloc");
			return (UPERF_FAILURE);
		}
		if ((ensure_read(p, si, size)) <= 0) {
			free(si);
			return (UPERF_FAILURE);
		}
		/* Now update the per-strand structures with this data */
		update_strand_with_slave_info(shm, si, host, g->nthreads, ssid);
		free(si);
	}

	return (UPERF_SUCCESS);
}

static int
handshake_phase2(uperf_shm_t *shm, workorder_t *w)
{
	int i, id;
	int status;
	group_t *g;

	id = 0;
	uperf_info("Starting handshake phase 2\n");
	for (i = 0; i < w->ngrp; i++) {
		g = &w->grp[i];
		protocol_t *p = g->control;
		while (p) {
			uperf_info("Handshake phase 2 with %s\n", p->host);
			status = handshake_p2_with_slave(shm, p->host, g, id,
			    p);
			if (status != UPERF_SUCCESS) {
				uperf_error("error handshaking with %s\n",
				    p->host);
				return (UPERF_FAILURE);
			}
			uperf_info("Handshake phase 2 with %s done\n", p->host);
			p = p->next;
		}
		id += g->nthreads;
	}

	return (UPERF_SUCCESS);

}

/*
 * During phase 1, connect to all hosts in all groups.
 * In phase 2 coalesce slaves, and send only once per group
 */
int
handshake(uperf_shm_t *shm, workorder_t *w)
{

	if ((handshake_phase1(w)) != UPERF_SUCCESS) {
		return (UPERF_FAILURE);
	}
	uperf_info("Completed handshake phase 1\n");
	if ((handshake_phase2(shm, w)) != UPERF_SUCCESS) {
		return (UPERF_FAILURE);
	}
	uperf_info("Completed handshake phase 2\n");
	return (UPERF_SUCCESS);
}

int
slave_handshake(uperf_shm_t *shm, protocol_t *p)
{
	int status = UPERF_SUCCESS;
	int i;
	int my_endian = UPERF_ENDIAN_VALUE;
	hs_phase1_t hsp1;
	hs_phase1_ack_t hsp1a;

	uperf_debug("%s begin\n", __func__);
	/* LINTED */
	while (1) {
		(void) memset(&hsp1a, 0, sizeof (hs_phase1_ack_t));
		(void) strlcpy(hsp1a.status, UPERF_OK, sizeof (hsp1a.status));

		uperf_debug("reading p1\n");
		if ((ensure_read(p, &hsp1, sizeof (hs_phase1_t))) <= 0) {
			uperf_debug("Cannot read p1\n");
			return (UPERF_FAILURE);
		}
		if (hsp1.endian != my_endian) {
			uperf_debug("Bit swapping hsp1\n");
			shm->bitswap = 1;
			hsp1.phase = BSWAP_32(hsp1.phase);
			hsp1.no_slave_info = BSWAP_32(hsp1.no_slave_info);
			for (i = 0; i < NUM_PROTOCOLS; i++)
				hsp1.protocol[i] = BSWAP_32(hsp1.protocol[i]);
		}
		if (strncmp(hsp1.magic, UPERF_MAGIC, sizeof (UPERF_MAGIC))
		    != 0) {
			(void) strlcpy(hsp1a.message, "Wrong magic",
					sizeof (hsp1a.message));
			(void) strlcpy(hsp1a.status, UPERF_NOT_OK,
					sizeof (hsp1a.status));
			status = UPERF_FAILURE;
		/* Disable version check. */
#if ENABLED_HANDSHAKE
		} else if (strncmp(hsp1.version, UPERF_DATA_VERSION,
			UPERF_VERSION_LEN) != 0) {
			(void) snprintf(hsp1a.message, sizeof (hsp1a.message),
		"Version %s is incompatible with master data version %s",
				UPERF_DATA_VERSION, hsp1.version);
			(void) strlcpy(hsp1a.status, UPERF_NOT_OK,
					sizeof (hsp1a.status));
			status = UPERF_FAILURE;

#endif
		} else {
			int i;
			for (i = 0; i < NUM_PROTOCOLS; i++) {
				if (hsp1.protocol[i] == 0)
					continue;
				if (valid_protocol(i) == -1) {
					(void) snprintf(hsp1a.message,
						sizeof (hsp1a.message),
						"%s:%s",
					    "Unsupported protocol",
						protocol_to_str(i));
					(void) printf("%s:%s",
					    "Unsupported protocol",
						protocol_to_str(i));
					(void) strlcpy(hsp1a.status,
						UPERF_NOT_OK,
						sizeof (hsp1a.status));
					status = UPERF_FAILURE;
				}
			}
		}

		if ((hsp1.phase == HANDSHAKE_PHASE_2) &&
			(status == UPERF_SUCCESS)) {
			break;
		}
		if ((ensure_write(p, &hsp1a, sizeof (hs_phase1_ack_t)))
		    <= 0) {
			(void) printf("Cannot write p1");
			return (UPERF_FAILURE);
		}
	}

	/*
	 * HANDSHAKE_PHASE_2
	 * Receive worklist, create threads, send back information
	 */
	shm->worklist = rx_group_t(p, my_endian);
	shm->rx_no_slave_info = hsp1.no_slave_info;
	uperf_debug("Slave: no_slave_info = %d\n", hsp1.no_slave_info);
	if (shm->bitswap == 1 && shm->worklist)
		group_bitswap(shm->worklist);
	/* print_group_t_recurse(shm->worklist); */
	if (shm->worklist)
		return (UPERF_SUCCESS);
	else
		return (UPERF_FAILURE);

}
int
slave_handshake_p2_complete(uperf_shm_t *shm, protocol_t *p)
{
	int i, j, size;
	slave_info_t *si;

	size = shm->rx_no_slave_info * sizeof (slave_info_t);
	si = malloc(size);

	if (si == NULL) {
		perror("malloc");
		return (UPERF_FAILURE);
	}
	if ((ensure_read(p, si, size)) <= 0) {
		free(si);
		return (UPERF_FAILURE);
	}
	for (i = 0; i < shm->rx_no_slave_info && shm->bitswap; i++) {
		for (j = 0; j < NUM_PROTOCOLS; j++) {
			si[i].port[j] = BSWAP_32(si[i].port[j]);
		}
	}
	/* Now update the per-strand structures with this data */
	update_strand_with_slave_info(shm, si, p->host,
	    shm->worklist->nthreads, 0);

	free(si);
	return (UPERF_SUCCESS);
}

int
slave_handshake_p2_success(slave_info_t *sl, int no, protocol_t *p, int bitswap)
{
	hs_phase2_ack_t hsp2a;
	int i, j;

	if (bitswap) {
		hsp2a.no_slave_info = BSWAP_32(no);
	}
	else
		hsp2a.no_slave_info = no;
	for (i = 0; i < no && bitswap; i++) {
		for (j = 0; j < NUM_PROTOCOLS; j++) {
			if (sl[i].port[j] >  0)
				sl[i].port[j] = BSWAP_32(sl[i].port[j]);
		}
	}
	(void) strlcpy(hsp2a.p1a.status, UPERF_OK, sizeof (hsp2a.p1a.status));
	if ((ensure_write(p, &hsp2a, sizeof (hs_phase2_ack_t))) <= 0) {
		return (UPERF_FAILURE);
	}
	if (no == 0)
		return (UPERF_SUCCESS);

	if ((ensure_write(p, sl, no * sizeof (slave_info_t))) <= 0) {
		return (UPERF_FAILURE);
	}

	return (UPERF_SUCCESS);

}
/* ARGSUSED */
int
slave_handshake_p2_failure(char *message, protocol_t *p, int myendian)
{
	hs_phase2_ack_t hsp2a;

	(void) strlcpy(hsp2a.p1a.message, message, 128);
	if ((ensure_write(p, &hsp2a, sizeof (hs_phase2_ack_t))) <= 0) {
		return (UPERF_FAILURE);
	}
	return (UPERF_SUCCESS);
}

static int
handshake_end_with_slave(protocol_t *p)
{
	hs_phase1_ack_t ack;

	(void) memset(&ack, 0, sizeof (hs_phase1_ack_t));
	if ((ensure_write(p, &ack, sizeof (hs_phase1_ack_t))) <= 0) {
		return (UPERF_FAILURE);
	}
	if ((ensure_read(p, &ack, sizeof (hs_phase1_ack_t))) <= 0) {
		return (UPERF_FAILURE);
	}

	return (UPERF_SUCCESS);
}

/* ARGSUSED */
int
handshake_end_master(uperf_shm_t *shm, workorder_t *w)
{
	int i;
	group_t *g;

	for (i = 0; i < w->ngrp; i++) {
		g = &w->grp[i];
		protocol_t *p = g->control;
		while (p) {
			if (handshake_end_with_slave(p) != UPERF_SUCCESS) {
				uperf_error("Error handshaking P3 with %s\n",
				    p->host);
				return (UPERF_FAILURE);
			}
			p = p->next;
		}
	}

	return (UPERF_SUCCESS);
}

/* ARGSUSED */
int
handshake_end_slave(slave_info_t *sl, int no, protocol_t *p, int bitswap)
{
	hs_phase1_ack_t ack;

	if ((ensure_read(p, &ack, sizeof (hs_phase1_ack_t))) <= 0) {
		return (UPERF_FAILURE);
	}
	if ((ensure_write(p, &ack, sizeof (hs_phase1_ack_t))) <= 0) {
		return (UPERF_FAILURE);
	}

	return (UPERF_SUCCESS);
}
