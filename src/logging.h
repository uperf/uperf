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

#ifndef	 _LOGGING_H
#define	 _LOGGING_H

/*
 * Logging.
 * Every error/warning is reported to the global error log using
 * uperf_log(type, errno, msg). The logger stores upto NUM_ERR_STR
 * messages, and if full, the log is flushed.
 * If the previous message is the same as the current message, 
 * the count for the previous message is incremented, and this
 * message is discarded.
 *
 * Periodic flushes of the log can also be accomplished using
 * uperf_log_flush()
 *
 * Log messages are of type <msg, count> where count is the 
 * number of times this message was successively received
 */
#include <pthread.h>

#define	NUM_ERRNOS	256
#define	NUM_ERR_STR	256
#define	ERR_STR_LEN	512

typedef enum {
	UPERF_NONVERBOSE = 0,		/* Prints only critical msgs */
	UPERF_VERBOSE = 2,		/* Prints misc info msgs */
} uperf_log_level;

typedef enum {
	UPERF_LOG_ERROR = 0,
	UPERF_LOG_QUIT,
	UPERF_LOG_ABORT,
	UPERF_LOG_INFO,
	UPERF_LOG_DEBUG,
	UPERF_LOG_WARN
}uperf_msg_type;

typedef struct {
	pthread_mutex_t lock;
	pthread_mutexattr_t attr;
	struct _errstr {
		int myerrno;
		uperf_msg_type type;
		int count;
		char str[ERR_STR_LEN];
	}msg[NUM_ERR_STR];
	int num_msg;
} uperf_log_t;

int uperf_log_init(uperf_log_t *);
int uperf_log_flush();
int uperf_log_flush_to_string(char *, int);
int uperf_log_msg(uperf_msg_type, int, char *);
int ulog(uperf_msg_type type, int myerrno, char *fmt, ...);
int uperf_log_num_msgs();
void uperf_printer(uperf_log_level, uperf_msg_type, const char *, ...);
int uperf_set_log_level(uperf_log_level);

/*
 * Macros that define the error functions and maps back to uperf_printer
 * basic function
 */
#define uperf_error(...)	uperf_printer(UPERF_NONVERBOSE,		\
    UPERF_LOG_ERROR,__VA_ARGS__);
#define uperf_fatal(...)  	{uperf_printer(UPERF_NONVERBOSE, 	\
    UPERF_LOG_ERROR,__VA_ARGS__);	exit(1); }
#define uperf_quit(...)  	{uperf_printer(UPERF_NONVERBOSE, 	\
    UPERF_LOG_QUIT,__VA_ARGS__);	exit(1); }
#define uperf_abort(...)   	{uperf_printer(UPERF_NONVERBOSE, 	\
    UPERF_LOG_ABORT,__VA_ARGS__); abort();exit(1);}
#define uperf_warn(...)  	{uperf_printer(UPERF_VERBOSE, 		\
    UPERF_LOG_WARN,	__VA_ARGS__);}
#define uperf_info(...)   	{uperf_printer(UPERF_VERBOSE, 		\
    UPERF_LOG_INFO,	__VA_ARGS__);}

#define ulog_err(...)	ulog(UPERF_LOG_ERROR, errno, __VA_ARGS__);
#define ulog_warn(...)	ulog(UPERF_LOG_WARN, errno, __VA_ARGS__);
#ifdef DEBUG
#define uperf_debug(...)  	{uperf_printer(UPERF_NONVERBOSE, 	\
    UPERF_LOG_DEBUG, __VA_ARGS__);}
#else
#define uperf_debug(...)
#endif

#endif /* _LOGGING_H */
