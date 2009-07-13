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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved. Use is
 * subject to license terms.
 */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#include "uperf.h"
#include "hwcounter.h"
#include <sys/systeminfo.h>
#include "logging.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

enum event {
	VALID, NOTVALID
};
static int valid_event = NOTVALID;

static void
check_event(void *arg, uint_t picno, const char *event)
{
	char *ev = (char *) arg;

	uperf_debug("Checking %s == %s\n", ev, event);
	if (valid_event == NOTVALID && (strcasecmp(ev, event) == 0)) {
		valid_event = VALID;
	}
}

#ifdef DEBUG
static void
print_event(void *arg, const char *event)
{
	printf("Event is :%s\n", event);
}
#endif /* DEBUG */

int
hwcounter_init()
{
#ifdef USE_CPCv11
	int cpuver;

	if (cpc_version(CPC_VER_CURRENT) != CPC_VER_CURRENT) {
		(void) fprintf(stderr, "CPC library mismatch\n");
		return (1);
	}
#elif defined(USE_CPCv21)
	if ((cpc = cpc_open(CPC_VER_CURRENT)) == NULL) {
		char *errstr = strerror(errno);
		(void) fprintf(stderr,
		    "cannot access performance counter library - %s\n",
		    errstr);
		return (1);
	}
#endif
	return (0);
}
int
hwcounter_fini(hwcounter_t * hw)
{
#ifdef USE_CPCv21
	return (cpc_close(cpc));
#else
	return (0);
#endif
}
/*
 * We first try to get the events from UPERFHWEVENTS environment variable.
 * If not, we fall back to the the following
 */
static void
default_counters(hwcounter_t * hw)
{
	char arch[128];
	char *ev1, *ev2;
	char *events;
	int use_defaults = 0;

	if ((events = getenv("UPERFHWEVENTS")) == NULL) {
		use_defaults = 1;
	} else {
		ev1 = strtok(events, ",");
		ev2 = strtok(NULL, ",");
		if ((ev1 == NULL) || (ev2 == NULL)) {
			(void) fprintf(stderr,
			    "I Dont understand UPERFHWEVENTS!!");
			(void) fprintf(stderr, "We will use defaults\n");
			use_defaults = 1;
		}
	}
	if (use_defaults) {
		/* get the arch */
		sysinfo(SI_ARCHITECTURE, arch, 128);
		/* fprintf(stderr, "DEBUG: arch:%s\n", arch); */
		if (strcmp(arch, "i386") == 0) {
			ev1 = "BU_cpu_clk_unhalted";
			ev2 = "FR_retired_x86_instr_w_excp_intr";
		} else if (strcmp(arch, "sparc") == 0) {
			ev1 = "Cycle_cnt";
			ev2 = "Instr_cnt";
		} else {
			ev1 = ev2 = "Error";
		}
	}
	strlcpy(hw->counter1, ev1, COUNTER_MAXLEN);
	strlcpy(hw->counter2, ev2, COUNTER_MAXLEN);
}
/*
 * Validate the events ev1 and ev2. Return 0 on success Called in single
 * thread mode. Updates valid_event
 */
int
hwcounter_validate_events(char *ev1, char *ev2)
{
#ifdef USE_CPCv2
	cpc_t *cpc;
	char *errstr;

	/* NULL events are OK as we will use default ones */
	if (ev1 == NULL || ev2 == NULL) {
		return (0);
	}
	if ((cpc = cpc_open(CPC_VER_CURRENT)) == NULL) {
		errstr = strerror(errno);
		(void) fprintf(stderr,
		    "cannot access performance counter library - %s\n",
		    errstr);
		return (1);
	}
#ifdef DEBUG
	printf("No of counters:%d\nValid events are", cpc_npic(cpc));
	cpc_walk_events_all(cpc, 0, print_event);
#endif
	cpc_walk_events_pic(cpc, 0, ev1, check_event);
	if (valid_event == VALID) {
		cpc_walk_events_pic(cpc, 1, ev2, check_event);
	}
	cpc_close(cpc);
#endif
	if (valid_event == NOTVALID) {
		return (1);
	} else {
		return (0);
	}
}
int
hwcounter_initlwp(hwcounter_t * hw, const char *c1, const char *c2)
{
	int ret, cpuver;
	char *eventstr;
	char *errstr;
	int siz = 512;

	hw->init_status = -1;
	if (c1 == NULL || c2 == NULL) {
		default_counters(hw);
	} else {
		strlcpy(hw->counter1, c1, COUNTER_MAXLEN);
		strlcpy(hw->counter2, c2, COUNTER_MAXLEN);
	}
#ifdef USE_CPCv1
	if (cpc_version(CPC_VER_CURRENT) != CPC_VER_CURRENT) {
		(void) fprintf(stderr, "CPC library mismatch\n");
		return (-1);
	}
	eventstr = calloc(siz, sizeof (char));
	if (eventstr == NULL) {
		(void) fprintf(stderr,
		    "Cannot allocate memory for event string.\n");
		return (-1);
	}
	snprintf(eventstr, siz, "pic0=%s,pic1=%s", hw->counter1, hw->counter2);
	if ((cpuver = cpc_getcpuver()) == -1) {
		(void) fprintf(stderr, "No performance counter hardware!");
		free(eventstr);
		return (-1);
	}
	if (cpc_strtoevent(cpuver, eventstr, &hw->event) != 0) {
		(void) fprintf(stderr, "can't measure '%s' on this processor",
			eventstr);
		free(eventstr);
		return (-1);
	}
	if (cpc_access() == -1) {
		(void) fprintf(stderr, "Cannot  access perf counters: %s",
			strerror(errno));
		free(eventstr);
		return (-1);
	}
	if (cpc_bind_event(&hw->event, 0) == -1) {
		(void) fprintf(stderr, "Cannot bind lwp%d: %s",
			MY_THREAD_ID(), strerror(errno));
		free(eventstr);
		return (-1);
	}
#elif defined(USE_CPCv2)
	assert(hw);
	if ((hw->cpc = cpc_open(CPC_VER_CURRENT)) == NULL) {
		errstr = strerror(errno);
		(void) fprintf(stderr,
		    "cannot access performance counter library - %s\n",
		    errstr);
		return (-1);
	}
	hw->set = cpc_set_create(hw->cpc);
	/*
	 * printf("No of counters:%d\n", cpc_npic(cpc));
	 * cpc_walk_events_all(cpc, 0, print_event);
	 */
	ret = cpc_set_add_request(hw->cpc,	/* ptr to cpc_t */
		hw->set,	/* The set */
		hw->counter1,	/* Event */
		0,		/* Initial val of counter */
		CPC_COUNT_USER | CPC_COUNT_SYSTEM,	/* flag */
		0, NULL);
	if (ret == -1) {
		(void) fprintf(stderr, "Error adding counter %s to set\n",
			hw->counter1);
		perror("cpc_set_add_request:");
		(void) cpc_set_destroy(hw->cpc, hw->set);
		return (-1);
	}
	/* fprintf(stderr, "DEBUG: added event at index : %d\n", ret); */
	ret = cpc_set_add_request(hw->cpc,	/* ptr to cpc_t */
		hw->set,	/* The set */
		hw->counter2,	/* Event */
		0,		/* Initial val of counter */
		CPC_COUNT_USER | CPC_COUNT_SYSTEM,	/* flag */
		0, NULL);
	if (ret == -1) {
		(void) fprintf(stderr, "Error adding counter %s to set\n",
			hw->counter2);
		perror("cpc_set_add_request:");
		(void) cpc_set_destroy(hw->cpc, hw->set);
		return (-1);
	}
	/* Now bind the current lwp to this event set */
	ret = cpc_bind_curlwp(hw->cpc, hw->set, CPC_BIND_LWP_INHERIT);
	if (ret != 0) {
		(void) fprintf(stderr, "Error binding current lwp\n");
		perror("cpc_bin_curlwp:");
		(void) cpc_set_destroy(hw->cpc, hw->set);
		return (-1);
	}
	hw->start = cpc_buf_create(hw->cpc, hw->set);
	hw->end = cpc_buf_create(hw->cpc, hw->set);
	hw->diff = cpc_buf_create(hw->cpc, hw->set);
	if (hw->start == NULL || hw->end == NULL || hw->diff == NULL) {
		(void) fprintf(stderr, "Error creating buffers\n");
		perror("cpc_bin_curlwp:");
		assert(!cpc_unbind(hw->cpc, hw->set));
		(void) cpc_set_destroy(hw->cpc, hw->set);
		return (-1);
	}
#endif
	hw->init_status = 1;
#ifdef USE_CPCv1
	free(eventstr);
#endif
	return (0);
}
int
hwcounter_finilwp(hwcounter_t * hw)
{
#ifdef USE_CPCv2
	if (hw->init_status == 1) {
		cpc_unbind(hw->cpc, hw->set);
		(void) cpc_set_destroy(hw->cpc, hw->set);
	}
#endif
	return (0);
}
#ifdef USE_CPCv2
static void
print_cpc_buf(cpc_t * cpc, cpc_buf_t * buf)
{
	uint64_t val;

	cpc_buf_get(cpc, buf, 0, &val);
	(void) printf("Counter1: %9lld\n", val);
	cpc_buf_get(cpc, buf, 1, &val);
	(void) printf("Counter1: %9lld\n", val);
}
#endif
int
hwcounter_snap(hwcounter_t * hw, int type)
{
	int err = 0;

	assert(hw->init_status);
	if (type == SNAP_BEGIN) {
		/* fprintf(stderr, "SNAP_BEGIN SNAP\n"); */
#ifdef USE_CPCv1
		err = cpc_take_sample(&hw->start);
#elif defined(USE_CPCv2)
		err = cpc_set_sample(hw->cpc, hw->set, hw->start);
#endif
	} else {
		/* fprintf(stderr, "END SNAP\n"); */
#ifdef USE_CPCv1
		err = cpc_take_sample(&hw->end);
#elif defined(USE_CPCv2)
		err = cpc_set_sample(hw->cpc, hw->set, hw->end);
#endif
	}
	if (err != 0)
		(void) fprintf(stderr, "Error snapping %s\n", strerror(errno));
	return (0);
}
/*
 * Get the counter index = 0 : Counter1, usr+sys index = 1 : Counter2,
 * usr+sys
 */
uint64_t
hwcounter_get(hwcounter_t * hw, int index)
{
	uint64_t val;

#ifdef USE_CPCv1
	switch (index) {
	case 0:
		val = hw->end.ce_pic[0] - hw->start.ce_pic[0];
	case 1:
		val = hw->end.ce_pic[1] - hw->start.ce_pic[1];
	}
#elif defined(USE_CPCv2)
	cpc_buf_sub(hw->cpc, hw->diff, hw->end, hw->start);
	cpc_buf_get(hw->cpc, hw->diff, index, &val);
#endif
	return (val);
}
#ifdef HWCOUNTER_MAIN
int
main(int argc, char *argv[])
{
	hwcounter_t *hw;
	int i, total = 0;

	hwcounter_init();
	hw = hwcounter_initlwp(NULL, NULL);
	hwcounter_snap(hw, SNAP_BEGIN);
	for (i = 0; i < 20; i++)
		total++;
	hwcounter_snap(hw, SNAP_END);
	printf("Iteration count: %d\n", total);
	printf("%9s %9s\n", "Cycle_cnt", "Instr_cnt");
	printf("%9lld %9lld\n", hwcounter_get(hw, 0),
		hwcounter_get(hw, 1));
	hwcounter_finilwp(hw);
	hwcounter_fini(hw);
}
#endif				/* HWCOUNTER_MAIN */
