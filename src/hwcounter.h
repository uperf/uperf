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

#ifndef _HWCOUNTER_H
#define _HWCOUNTER_H

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#ifdef USE_CPC
#include <libcpc.h>
#endif /* USE_CPC */

#define COUNTER_MAXLEN	64

#ifdef USE_CPC
/* On Solaris 9, we use CPCv1. On others we use CPCv2 */
#if 0
#if defined(__SunOS_5_9) || defined (__SunOS_5_8)
#define USE_CPCv1
#else
#define USE_CPCv2
#endif
#endif
typedef struct {
#ifdef USE_CPCv1
	cpc_event_t	event;
	cpc_event_t	start, end;
#elif defined(USE_CPCv2)
	cpc_t		*cpc;
	cpc_set_t 	*set;
	cpc_buf_t *start, *end, *diff;
#endif
	char 		counter1[COUNTER_MAXLEN];
	char 		counter2[COUNTER_MAXLEN];
	uint64_t	ucounter1;
	uint64_t	scounter1;
	uint64_t	ucounter2;
	uint64_t	scounter2;
	int		init_status;
}hwcounter_t;

int hwcounter_init();
int hwcounter_fini();
int hwcounter_validate_events(char *, char *);
int hwcounter_finilwp(hwcounter_t *);
int hwcounter_snap(hwcounter_t *, int);
int hwcounter_initlwp(hwcounter_t *, const char *, const char *);

/* Get the counter
 * index = 0 : Counter1, usr+sys
 * index = 1 : Counter2, usr+sys
 */
uint64_t hwcounter_get(hwcounter_t *, int);
#endif /* USE_CPC */

#endif /* _HWCOUNTER_H */
