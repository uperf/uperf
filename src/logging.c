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

/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved. Use is
 * subject to license terms.
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#include "uperf.h"

#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "logging.h"
#ifdef HAVE_SYS_VARARGS_H
#include <sys/varargs.h>
#endif /* HAVE_SYS_VARARGS_H */


#ifdef UPERF_SOLARIS
#ifndef __builtin_stdarg_start
#define __builtin_stdarg_start __builtin_va_start
#endif
#endif


static int log_level;	/* Log level */
static uperf_log_t *log;

int
uperf_set_log_level(uperf_log_level level)
{
	log_level = level;
	return (0);

}
/* ARGSUSED */
void
uperf_printer(uperf_log_level level, uperf_msg_type flag, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (level <= log_level) {
		(void) vprintf(fmt, ap);
	}
	va_end(ap);
}

int
uperf_log_init(uperf_log_t *l)
{
	log = l;
	(void) memset(log, 0, sizeof (uperf_log_t));
	if (pthread_mutexattr_init(&log->attr) != 0) {
		perror("pthread_mutexattr_init");
		return (UPERF_FAILURE);
	}
#ifndef STRAND_THREAD_ONLY
	if (pthread_mutexattr_setpshared(&log->attr,
		PTHREAD_PROCESS_SHARED) != 0) {
		perror("pthread_mutexattr_setpshared");
		return (UPERF_FAILURE);
	}
#endif /* STRAND_THREAD_ONLY */
	if (pthread_mutex_init(&log->lock, &log->attr) != 0) {
		perror("pthread_mutex_init");
		return (UPERF_FAILURE);
	}

	return (UPERF_SUCCESS);
}

int
uperf_log_num_msgs()
{
	return (log->num_msg);
}

int
uperf_log_flush_to_string(char *buffer, int size)
{
	int i;
	int index = 0;

	(void) pthread_mutex_lock(&log->lock);
	for (i = 0; i < log->num_msg; i++) {
		int err = log->msg[i].myerrno;
		char *e;

		e = ((err == 0) ?  " " : strerror(err));
		if (strlen(log->msg[i].str) == 0)
			continue;
		if (log->msg[i].count > 2) {
			char *t;
			if (log->msg[i].type == UPERF_LOG_WARN)
				t = "Warnings";
			else
				t = "Errors";
			(void) snprintf(&buffer[index], size - index,
				"%d %s  %s %s\n",
				log->msg[i].count, t,
				log->msg[i].str, e);
			index = strlen(buffer);
		} else {
			if (log->msg[i].type == UPERF_LOG_WARN) {
				(void) snprintf(&buffer[index], size - index,
					"Warning: ");
			} else {
				(void) snprintf(&buffer[index], size - index,
					" ");
			}
			index = strlen(buffer);
			(void) snprintf(&buffer[index], size - index,
				"%s %s\n", log->msg[i].str, e);
			index = strlen(buffer);
		}
	}
	(void) pthread_mutex_unlock(&log->lock);

	return (UPERF_SUCCESS);
}


int
uperf_log_flush()
{
	int i;

	(void) pthread_mutex_lock(&log->lock);
	for (i = 0; i < log->num_msg; i++) {
		int err = log->msg[i].myerrno;
		char *e;
		e = ((err == 0) ?  " " : strerror(err));
		if (strlen(log->msg[i].str) == 0)
			continue;
		if (log->msg[i].count > 2) {
			char *t;
			if (log->msg[i].type == UPERF_LOG_WARN)
				t = "Warnings";
			else
				t = "Errors";
			(void) printf("** %d %s %s %s\n",
			    log->msg[i].count, t,
			    log->msg[i].str, e);
		} else {
			if (log->msg[i].type == UPERF_LOG_WARN)
				(void) printf("** Warning: ");
			else
				(void) printf("** ");
			(void) printf("%s %s\n", log->msg[i].str, e);
		}
	}
	/* reset the counters */
	(void) memset(log, 0, sizeof (uperf_log_t));
	(void) pthread_mutex_unlock(&log->lock);

	return (UPERF_SUCCESS);
}

int
ulog(uperf_msg_type type, int myerrno, char *fmt, ...)
{
	char buf[ERR_STR_LEN];
	va_list ap;

	va_start(ap, fmt);
	(void) vsnprintf(buf, sizeof (buf), fmt, ap);
	va_end(ap);

	return (uperf_log_msg(type, myerrno, buf));
}

int
uperf_log_msg(uperf_msg_type type, int myerrno, char *msg)
{
	int no;
	int i;

	if (!msg || myerrno == EINTR)
		return (UPERF_SUCCESS);

	uperf_debug("logging: %d %s\n", type, msg);

	(void) pthread_mutex_lock(&log->lock);
	no = log->num_msg;
	(void) pthread_mutex_unlock(&log->lock);

	if (no > NUM_ERR_STR) {
		(void) uperf_log_flush();
	}

	(void) pthread_mutex_lock(&log->lock);

	for (i = 0; i < log->num_msg; i++) {
		if (strncmp(log->msg[i].str, msg, ERR_STR_LEN) == 0) {
			log->msg[i].count++;
			(void) pthread_mutex_unlock(&log->lock);
			errno = myerrno;
			return (UPERF_SUCCESS);
		}
	}
	(void) strlcpy(log->msg[log->num_msg].str, msg, ERR_STR_LEN - 1);
	log->msg[log->num_msg].str[ERR_STR_LEN-1] = '\0';
	log->msg[log->num_msg].count = 1;
	log->msg[log->num_msg].myerrno = myerrno;
	log->msg[log->num_msg].type = type;
	log->num_msg++;

	(void) pthread_mutex_unlock(&log->lock);

	/* reset errno to its old value */
	errno = myerrno;

	return (UPERF_SUCCESS);
}
