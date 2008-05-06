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

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>

#include "numbers.h"
#include "print.h"

#define	NETSTAT_FIELDS  17
#define	MAX_NETS	256
#define	dprintf

#ifdef HAVE_LIBKSTAT
#include <kstat.h>

#define	WARNSTR	"WARNING: kstat state changed. kstat numbers may be wrong\n"
static kstat_named_t nets[MAX_NETS][NETSTAT_FIELDS];
static kstat_ctl_t   *kc;
static int state_changed = 0;

#endif /* HAVE_LIBKSTAT */

#define	INTERFACE_LEN	32
struct packet_stats {
	uint64_t	tx_pkts;
	uint64_t	rx_pkts;
	uint64_t	tx_bytes;
	uint64_t	rx_bytes;
	hrtime_t	stamp;
};

struct _nic {
	struct packet_stats	s[2];
	char 			interface[INTERFACE_LEN];
#ifdef HAVE_LIBKSTAT
	kstat_t 		*ksp;
#endif /* HAVE_LIBKSTAT */
};
static struct _nic	nics[MAX_NETS];
static int		no_nics = 0;

#ifdef HAVE_LIBKSTAT
/*
 * Return value of named statistic for given kstat_named kstat;
 * return 0LL if named statistic is not in list (use "ll" as a
 * type qualifier when printing 64-bit int's with printf() )
 */
static uint64_t
kstat_named_value(kstat_t *ksp, char *name)
{
	kstat_named_t *knp;
	uint64_t value;

	if (ksp == NULL)
		return (0LL);

	knp = kstat_data_lookup(ksp, name);
	if (knp == NULL)
		return (0LL);

	switch (knp->data_type) {
	case KSTAT_DATA_INT32:
	case KSTAT_DATA_UINT32:
		value = (uint64_t)(knp->value.ui32);
		break;
	case KSTAT_DATA_INT64:
	case KSTAT_DATA_UINT64:
		value = knp->value.ui64;
		break;
	default:
		value = 0LL;
		break;
	}

	return (value);
}

int
netstat_init()
{
	kstat_t *ksp;

	kc = kstat_open();

	if (kstat_chain_update(kc))
		(void) fprintf(stderr, "<<State Changed>>n");
	for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next) {
		if (ksp->ks_type != KSTAT_TYPE_NAMED ||
		    strcmp(ksp->ks_class, "net") != 0 ||
		    kstat_read(kc, ksp, NULL) == -1 ||
		    kstat_data_lookup(ksp, "ipackets64") == NULL ||
		    kstat_data_lookup(ksp, "opackets64") == NULL)
			continue;
		strlcpy(nics[no_nics].interface, ksp->ks_name, INTERFACE_LEN);
		nics[no_nics++].ksp = ksp;
	}

	return (0);
}


void
netstat_snap(int snaptype)
{
	int		i, index;
	kstat_t		*ksp;
	kid_t		kstat_chain_id;

	if (kstat_chain_update(kc))
		state_changed = 1;
	if (snaptype == SNAP_BEGIN)
		index = 0;
	else
		index = 1;
	for (i = 0; i < no_nics; i++) {
		ksp = nics[i].ksp;
		kstat_chain_id = kstat_read(kc, ksp, nets[0]);
		nics[i].s[index].tx_pkts
		    = kstat_named_value(ksp, "opackets64");
		nics[i].s[index].rx_pkts
		    = kstat_named_value(ksp, "ipackets64");
		nics[i].s[index].tx_bytes = kstat_named_value(ksp, "obytes64");
		nics[i].s[index].rx_bytes = kstat_named_value(ksp, "rbytes64");
		nics[i].s[index].stamp = gethrtime();

	}
}

#endif /* HAVE_LIBKSTAT */

#ifdef UPERF_LINUX

/*
 * Linux stores the netstat data in /proc/net/data. First
 * two lines are headers, and then there is a one line per
 * interface
 */
#define	NETSTAT_DEV	"/proc/net/dev"
#define	NETSTAT_SEP	" |:"

static int i_rbytes, i_rpkts, i_tbytes, i_tpkts;

int
netstat_init()
{
	FILE *f;
	char buffer[1024];
	char *token;
	int index;
	struct stat buf;

	if (stat(NETSTAT_DEV, &buf) != 0) {
		printf("Cannot stat %s\n", NETSTAT_DEV);
		return (UPERF_FAILURE);
	}

	if ((f = fopen(NETSTAT_DEV, "r")) == NULL) {
		printf("Cannot open %s\n", NETSTAT_DEV);
		return (UPERF_FAILURE);
	}
	i_rbytes = i_rpkts = i_tbytes = i_tpkts = 0;
	fgets(buffer, 1024, f); /* Ignore first line */
	fgets(buffer, 1024, f);

	token = strtok(buffer, NETSTAT_SEP);
	index = 0;
	while (token) {
		if (strcmp(token, "packets") == 0) {
			if (i_rpkts == 0) {
				i_rpkts = index;
			} else {
				i_tpkts = index;
			}
		} else if (strcmp(token, "bytes") == 0) {
			if (i_rbytes == 0) {
				i_rbytes = index;
			} else {
				i_tbytes = index;
			}
		}
		token = strtok(NULL, NETSTAT_SEP);
		index++;
	}
	if ((i_rpkts == i_tpkts) || (i_rbytes == i_tbytes)) {
		printf("Error parsing header from %s\n", NETSTAT_DEV);
		return (UPERF_FAILURE);
	}
	fclose(f);

	return (UPERF_SUCCESS);
}
static int
netstat_parse(char line[], struct packet_stats *ps)
{
	char *token;
	int index = 0;

	ps->tx_pkts = ps->rx_pkts = ps->tx_bytes = ps->rx_bytes = 0;
	token = strtok(line, NETSTAT_SEP);
	while (token) {
		if (index == i_rbytes) {
			ps->rx_bytes = atol(token);
		} else if (index == i_tbytes) {
			ps->tx_bytes = atol(token);
		} else if (index == i_tpkts) {
			ps->tx_pkts = atol(token);
		} else if (index == i_rpkts) {
			ps->rx_pkts = atol(token);
		}
		token = strtok(NULL, NETSTAT_SEP);
		index++;
	}
	ps->stamp = GETHRTIME();

	return (UPERF_SUCCESS);
}
static int
find_nic(char *name)
{
	int i;
	for (i = 0; i < no_nics; i++)
		if (strncmp(nics[i].interface, name, INTERFACE_LEN) == 0)
			return (i);

	return (-1);
}

int
netstat_snap(int snaptype)
{
	FILE *f;
	char buffer[1024], hdr[1024];
	char *interface, *tmp;
	struct packet_stats ps;

	if ((f = fopen(NETSTAT_DEV, "r")) == NULL) {
		printf("Cannot open %s\n", NETSTAT_DEV);
		return (UPERF_FAILURE);
	}
	/* ignore headers */
	fgets(buffer, 1024, f);
	fgets(buffer, 1024, f);

	while (fgets(buffer, 1024, f) > 0) {
		strncpy(hdr, buffer, 1024);
		interface = strtok(hdr, NETSTAT_SEP);

		if (snaptype == SNAP_BEGIN) {
			strncpy(nics[no_nics].interface, interface,
				INTERFACE_LEN);
			netstat_parse(buffer, &nics[no_nics++].s[0]);
		} else {
			int ind = find_nic(interface);
			if (ind < 0) {
				printf("Nic %s came online", interface);
				continue;
			} else {
				netstat_parse(buffer, &nics[ind].s[1]);
			}
		}
	}
	fclose(f);

	return (UPERF_SUCCESS);
}
#endif /* UPERF_LINUX */
void
print_netstat()
{
	int 		i;
	uint64_t	op, ip, ob, ib;
	double		t;

	(void) printf("\nNetstat statistics for this run\n");

#ifdef HAVE_LIBKSTAT
	if (state_changed == 1) {
		(void) printf(WARNSTR);
	}
#endif /* HAVE_LIBKSTAT */

	(void) uperf_line();
	(void) printf("%-5s  %10s  %10s  %10s  %10s\n",
	    "Nic", "opkts/s", "ipkts/s", "obits/s", "ibits/s");
	for (i = 0; i < no_nics; i++) {
		op = nics[i].s[1].tx_pkts - nics[i].s[0].tx_pkts;
		ip = nics[i].s[1].rx_pkts - nics[i].s[0].rx_pkts;
		ib = nics[i].s[1].rx_bytes - nics[i].s[0].rx_bytes;
		ob = nics[i].s[1].tx_bytes - nics[i].s[0].tx_bytes;
		t =  (nics[i].s[1].stamp -  nics[i].s[0].stamp)/1.0e+9;
		(void) printf("%-5s  %10.0f  %10.0f ",
		    nics[i].interface, op/t, ip/t);
		PRINT_NUMb(ob*8/t, 10);
		PRINT_NUMb(ib*8/t, 10);
		printf("\n");
	}
	(void) uperf_line();
}
