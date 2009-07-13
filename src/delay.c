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
 * along with uperf.  If not, see < http://www.gnu.org/licenses/>.
 */

#ifdef	HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#include <time.h>
#include <math.h>
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include "uperf.h"
#include "logging.h"

/* Returns 0 on success */
int
uperf_sleep(hrtime_t duration_nsecs)
{
#ifdef HAVE_NANOSLEEP
	struct timespec rqtp, rmtp;
	rqtp.tv_sec = (long) duration_nsecs/1.0e+9;
	rqtp.tv_nsec = (long) fmod(duration_nsecs, 1.0e+9);
	if (nanosleep(&rqtp, &rmtp) == -1) {
		char msg[512];
		int saved_errno = errno;
		if (saved_errno != EINTR) {
			(void) snprintf(msg, 512,
			    "Error sleeping for %.4fs\n",
			    duration_nsecs*1.0/1.0e+9);
			uperf_log_msg(UPERF_LOG_ERROR, saved_errno, msg);
		}
		return (saved_errno);
	}
	return (0);
#else
	if (duration_nsecs/1.0e+6 < 1)
		return (0);
	return (poll(NULL, NULL, (int)(duration_nsecs/1.0e+6))
	    == 0 ? 0 : errno);
#endif
}

int
uperf_spin(hrtime_t duration)
{
#define	PER_LOOP	10000
	hrtime_t start;
	int i;
	long sum = 0;

	start = GETHRTIME();
	while (((GETHRTIME() - start)) < duration) {
		for (i = 0; i < PER_LOOP; i++) {
			sum += i;
		}
	}

	return (0);
}
