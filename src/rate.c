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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>

#include "uperf.h"
#include "delay.h"

#define	INTERVALS_PER_SEC	2
#define	TIMESHIFT		10
#define	TIME_EXPIRED(A)	((GETHRTIME() >> TIMESHIFT) > ((A) >> TIMESHIFT))

/* Runs for around 1s/INTERVALS_PER_SEC */
static int
rate_delta(void *a, void *b, int rate, int (*callback)(void *, void *))
{
	hrtime_t end, sleep_time;
	hrtime_t local_stop;
	int per_loop = rate/INTERVALS_PER_SEC;
	int i, ret;

	assert(rate > 0);

	if (per_loop == 0)
		per_loop = 1;

	/* Do atleast per_loop or until duration is passed */
	local_stop = GETHRTIME() + 1.0e+9/INTERVALS_PER_SEC;

	for (i = 0; i < per_loop; i++) {
		if (TIME_EXPIRED(local_stop))
			break;
		if ((ret = callback(a, b)) != 0) {
			return (ret);
		}
	}

	end = GETHRTIME();
	/* Sleep remainder of the interval */
	sleep_time = (hrtime_t) (local_stop - end);
	/* Tests with nanosleep indicate that nanosleep is atleast 2000ns */
	if (sleep_time > 2000)
		return (uperf_sleep(sleep_time));
	else
		return (0);
}

/*
 * This function returns in
 * roughly 1 sec (or less if duration is expired).
 *
 * Returns 0 on sucCESS OR Errno
 */
int
rate_execute_1s(void *a, void *b, int rate, int (*callback)(void *, void *))
{
	hrtime_t begin, end;
	int i, ret;

	assert(rate > 0);

	begin = GETHRTIME();
	ret = 0;
	for (i = 0; i < INTERVALS_PER_SEC && ret == 0; i++) {
		ret = rate_delta(a, b, rate, callback);
	}
	if (ret != 0)
		return (ret);
	else {
		end = GETHRTIME();
		if ((end - begin) < 1.0e+9)
			return (uperf_sleep(1.0e+9 - (end - begin)));
		return (ret);
	}
}

/* This function is busy wait version of rate_execute_1s */
int
rate_execute_1s_busywait(void *a, void *b, int rate,
	int (*callback)(void *, void *))
{
	hrtime_t begin, now;
	int i = 0;
	int ret = 0;
	int interval = 1.0e+9/rate;

	assert(rate > 0);
	begin = GETHRTIME();
	now = GETHRTIME();

	while ((now - begin) < 1.0e+9) {
		if ((now - begin) >= (interval * i)) {
			if ((ret = callback(a, b)) != 0) {
				return (ret);
			}
			i++;
		}
		now = GETHRTIME();
	}
	return (ret);
}
