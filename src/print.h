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

#ifndef _PRINT_H
#define	_PRINT_H

#include <sys/types.h>
#include "protocol.h"
#include "goodbye.h"
#include "workorder.h"

int print_goodbye_stat(char *host,  goodbye_stat_t *gstat);
int print_difference(goodbye_stat_t local, goodbye_stat_t remote);
void print_summary(newstats_t *ns, int same_line);
void print_txn_details(uperf_shm_t *shm);
void print_group_details(uperf_shm_t *shm);
void print_strand_details(uperf_shm_t *shm);
void print_txn_averages(uperf_shm_t *shm);
void print_flowop_averages(uperf_shm_t *shm);
void print_goodbye_stat_header();
int uperf_line();

#endif /* _PRINT_H */

