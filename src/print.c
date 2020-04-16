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

#include <string.h>
#include <stdio.h>
#include <strings.h>
#include "config.h"
#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif /* HAVE_TERMIO_H */
#ifdef HAVE_SYS_TTYCOM_H
#include <sys/ttycom.h>
#endif /* HAVE_SYS_TTYCOM_H */
#include <unistd.h>
#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif /* HAVE_STROPTS_H */
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */
#include <inttypes.h>
#include "uperf.h"
#include "print.h"
#include "main.h"
#include "workorder.h"
#include "strand.h"
#include "shm.h"
#include "goodbye.h"
#include "numbers.h"

extern options_t options;

#define	WINDOW_WIDTH	128

#define	AVG_HDR	"   Count         avg         cpu         max         min "

/* We calculate the width only on the first call to save repeated ioctls */
static int
window_width()
{
	static int width = 0;
	if (width > 0)
		return (width);
	else {
		struct winsize w;
		if (ioctl(fileno(stdout), TIOCGWINSZ, &w) != -1) {
			width = (w.ws_col ? w.ws_col : 80);
		if (width > 128)
			width = 128;
		} else {
			width = 80;
		}
	}

	return (width);
}
int
uperf_line()
{
	char msg[WINDOW_WIDTH];

	(void) memset(msg, '-', sizeof (msg));
	msg[window_width() - 1] = '\0';

	(void) printf("%s\n", msg);
	return (0);
}

static void
print_avg_header(char *label)
{
	printf("\n%-15s %s\n", label, AVG_HDR);
	uperf_line();
}

/* Txn1     41.73GB/29.40(s) = 12.19Gb/s 19288txn/s 51.84us/txn */
void
print_summary(newstats_t *ns, int same_line)
{
	double throughput;
	double ops;
	double time;
	double time_s;
	char msg[WINDOW_WIDTH];

	time = ns->end_time - ns->start_time;
	if (time <= 0) {
		return;
	}

	if (ENABLED_RAW_STATS(options)) {
		printf("timestamp_ms:%.4f name:%s nr_bytes:%"PRIu64" nr_ops:%"PRIu64"\n",
			ns->end_time/1.0e+6, ns->name, ns->size, ns->count);
	} else {
		time_s = time/1.0e+9;
		throughput = ns->size * 8.0 / time_s;
		ops = ns->count/time_s;
		if (same_line) {
			memset(msg, ' ', sizeof (msg));
			msg[window_width() - 1] = '\0';
			printf("\r%s\r", msg);
		}
		printf("%-8.8s ", ns->name);
		PRINT_NUM(ns->size, 8);
		printf("/%7.2f(s) = ", time/1.0e+9);
		PRINT_NUMb(throughput, 12);
		printf(" %10.0fop/s ", ops);
		if (same_line == 0)
			(void) printf("\n");
	}
	(void) fflush(stdout);
}

static void
print_average(newstats_t *ns)
{
	double avg;
	double cpu;

	if (!ns || ns->count == 0)
		return;
	avg = ns->time_used/ns->count;
	cpu = ns->cpu_time/ns->count;

	printf("%-15s %8"PRIu64" ", ns->name, ns->count);
	PRINT_TIME(avg, 11);
	PRINT_TIME(cpu, 11);
	PRINT_TIME(ns->max, 11),
	PRINT_TIME(ns->min, 11);

	if (ns->pic0 > 0) {
		printf("%-12.0f %-12.0f\n", (double) ns->pic0/ns->count,
		    (double) ns->pic1/ns->count);
	} else {
		printf("\n");
	}
}

/* Txn1 42.63GB/29.40(s) = 12.45Gb/s 19681txn/s */
void
print_txn_averages(uperf_shm_t *shm)
{
	int i, j;
	workorder_t *w = shm->workorder;
	group_t *g;
	txn_t *txn;
	newstats_t ns;

	print_avg_header("Txn");
	for (i = 0; i < w->ngrp; i++) {
		g = &w->grp[i];
		for (txn = g->tlist; txn; txn = txn->next) {
			bzero(&ns, sizeof (ns));
			ns.min = ULONG_MAX;
			ns.start_time = ULONG_MAX;
			for (j = 0; j < shm->nstat_count; j++) {
				newstats_t *p = &shm->nstats[j];
				if ((p->type == NSTAT_TXN) &&
				    (p->gid == GROUP_ID(g)) &&
				    (p->tid == TXN_ID(txn)))
					add_stats(&ns, p);
			}
			snprintf(ns.name, sizeof (ns.name), "Txn%d",
			    TXN_ID(txn));
			print_average(&ns);
		}
	}
	printf("\n");
}

/* Group0 42.63GB/29.40(s) = 12.45Gb/s 19681txn/s 50.81us/txn */
void
print_group_details(uperf_shm_t *shm)
{
	int i, j;
	workorder_t *w = shm->workorder;
	group_t *g;
	txn_t *txn;
	flowop_t *fptr;
	newstats_t ns;

	printf("\nGroup Details\n");
	uperf_line();
	for (i = 0; i < w->ngrp; i++) {
		g = &w->grp[i];
		bzero(&ns, sizeof (ns));
		ns.min = ULONG_MAX;
		ns.start_time = ULONG_MAX;
		strlcpy(ns.name, g->name, sizeof (ns.name));
		for (txn = g->tlist; txn; txn = txn->next) {
			for (fptr = txn->flist; fptr; fptr = fptr->next) {
				for (j = 0; j < shm->nstat_count; j++) {
					newstats_t *p = &shm->nstats[j];

					if ((p->type == NSTAT_FLOWOP) &&
					    (p->gid == GROUP_ID(g)) &&
					    (p->tid == TXN_ID(txn)))
						add_stats(&ns, p);
				}
			}
		}
		print_summary(&ns, 0);
	}
	printf("\n");
}

void
print_strand_details(uperf_shm_t *shm)
{
	int i;

	printf("\nStrand Details\n");
	uperf_line();
	for (i = 0; i < shm->no_strands; i++) {
		print_summary(STRAND_STAT(shm_get_strand(shm, i)), 0);
	}
	printf("\n");
}

void
print_flowop_averages(uperf_shm_t *shm)
{
	int i, j;
	workorder_t *w = shm->workorder;
	group_t *g;
	txn_t *txn;
	flowop_t *f;
	newstats_t ns;

	print_avg_header("Flowop");
	for (i = 0; i < w->ngrp; i++) {
		g = &w->grp[i];
		for (txn = g->tlist; txn; txn = txn->next) {
			for (f = txn->flist; f; f = f->next) {
				bzero(&ns, sizeof (ns));
				ns.min = ULONG_MAX;
				ns.start_time = ULONG_MAX;
				strlcpy(ns.name, f->name, sizeof (ns.name));
				for (j = 0; j < shm->nstat_count; j++) {
					newstats_t *p = &shm->nstats[j];
					if ((p->type == NSTAT_FLOWOP) &&
					    (p->gid == i) &&
					    (p->tid == TXN_ID(txn)) &&
					    (p->fid == FLOWOP_ID(f)))
						add_stats(&ns, p);
				}
				print_average(&ns);
			}
		}
	}
	printf("\n");
}

void
print_goodbye_stat_header()
{
	printf("\nRun Statistics\nHostname            Time       Data   \
Throughput   Operations      Errors\n");
	uperf_line();
}

/*
 * Print goodbye_stat
 * Sample:
 * localhost          11.08   264.56MB    200.26Mbs      4334526        0.00
 */
int
print_goodbye_stat(char *host,  goodbye_stat_t *gstat)
{
	double thro = 1.0;
	double err;

	if (gstat->elapsed_time > 0)
		thro = (gstat->bytes_xfer*8)/(gstat->elapsed_time/1.0e+9);
	err = (100.0 * gstat->error)/gstat->count;

	(void) printf("%-15.15s ", host);
	PRINT_TIME(gstat->elapsed_time, 8);
	PRINT_NUM((double)gstat->bytes_xfer, 10);
	PRINT_NUMb(thro, 12);
	printf("%12"PRIu64" %11.2f\n", gstat->count, err);

	return (0);
}

#define	ERR_PER(A, B) ((B) == 0 ? 0.0 : 100.0 - ((100.0 * (B))/(A)))

int
print_difference(goodbye_stat_t local, goodbye_stat_t remote)
{
	double ta, tb;
	double em, es;		/* Error master, error slave */

	ta = (local.bytes_xfer*8)/(local.elapsed_time/1.0e+9);
	tb = (remote.bytes_xfer*8)/(remote.elapsed_time/1.0e+9);
	em = (100.0 * local.error)/local.count;
	es = (100.0 * remote.error)/remote.count;
	(void) uperf_line();
	(void) printf("%-15s %7.2f%% %9.2f%% %11.2f%% %11.2f%% %10.2f%%\n\n",
		"Difference(%)",
		ERR_PER(local.elapsed_time, remote.elapsed_time),
		ERR_PER(local.bytes_xfer, remote.bytes_xfer),
		ERR_PER(ta, tb),
		ERR_PER(local.count, remote.count),
		MAX(em, es));
	return (0);
}
