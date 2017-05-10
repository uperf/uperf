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

#include "uperf.h"
#include "logging.h"

#ifdef UPERF_ANDROID
#include <hardware_legacy/power.h>
#include <sys/timerfd.h>
#endif /* UPERF_ANDROID */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif /* HAVE_POLL_H */


#define NSEC_PER_SEC 1000000000L
#define USEC_PER_SEC 1000000L

#if defined (UPERF_ANDROID) || defined (HAVE_NANOSLEEP)
static void nsecs_to_timespec(struct timespec *ts, hrtime_t duration_nsecs)
{
	ts->tv_sec = duration_nsecs / NSEC_PER_SEC;
	ts->tv_nsec = duration_nsecs % NSEC_PER_SEC;
}
#endif /* defined (UPERF_ANDROID) || defined (HAVE_NANOSLEEP) */

#ifdef UPERF_ANDROID
static int fd;

int init_android_alarm()
{
	fd = timerfd_create(CLOCK_BOOTTIME_ALARM, TFD_NONBLOCK);
	return fd;
}

static int set_wakeup_alarm(struct timespec *ts)
{
	struct itimerspec spec;

	memset(&spec, 0, sizeof(spec));

	spec.it_value.tv_sec = ts->tv_sec;
	spec.it_value.tv_nsec = ts->tv_nsec;

	return timerfd_settime(fd, 0, &spec, NULL);
}

static int __uperf_sleep(hrtime_t duration_nsecs)
{
	struct timespec ts;
	struct pollfd fds;
	int timeout_ms, ret;

	nsecs_to_timespec(&ts, duration_nsecs);
	if (set_wakeup_alarm(&ts) == -1) {
		return -1;
	}

	fds.fd = fd;
	fds.events = POLLIN;
	timeout_ms = duration_nsecs / USEC_PER_SEC;

	release_wake_lock(UPERF_WAKE_LOCK);
	ret = poll(&fds, 1, timeout_ms);
	acquire_wake_lock(PARTIAL_WAKE_LOCK, UPERF_WAKE_LOCK);

	return ret;
}
#elif defined (HAVE_NANOSLEEP)
static int __uperf_sleep(hrtime_t duration_nsecs)
{
	struct timespec rqtp, rmtp;
	nsecs_to_timespec(&rqtp, duration_nsecs);
	return nanosleep(&rqtp, &rmtp);
}
#else
static int __uperf_sleep(hrtime_t duration_nsecs)
{
	return poll(NULL, 0, (duration_nsecs / USEC_PER_SEC));
}
#endif /* UPERF_ANDROID */

/* Returns 0 on success */
int uperf_sleep(hrtime_t duration_nsecs)
{
	if (__uperf_sleep(duration_nsecs) == -1) {
		char msg[512];
		int saved_errno = errno;
		snprintf(msg, sizeof(msg), "Error sleeping for %.4fs\n", (duration_nsecs * 1.0 / NSEC_PER_SEC));
		uperf_log_msg(UPERF_LOG_ERROR, saved_errno, msg);
		return saved_errno;
	}

	return 0;
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
