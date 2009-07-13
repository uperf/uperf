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

#ifdef HAVE_SIGNAL_H
#include <signal.h> /* For SIGUSR2 */
#endif /* HAVE_SIGNAL_H */

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_WAIT_H
#include <wait.h>
#endif /* HAVE_WAIT_H */
#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif /* HAVE_SIGINFO_H */
#include "uperf.h"

/*
 * uperf uses the SIGUSR1 and SIGUSR2 signals.
 * SIGUSR2 is used to signal strands that a timeout has happened
 * (This is used in combination with "duration"
 *
 * SIGUSR1 is used to signal the thread to exit. We can use SIGKILL,
 * but there is no way to send a directed SIGKILL. Also, repeated
 * SIGKILLs to a thread that is on its way to die will kill the
 * main thread.
 *
 * When uperf is using processes (using the nprocs=X) feature, we
 * just use SIGKILL.
 *
 * SIGINT is also trapped to clean up and exit when the user hits
 * Cntrl-C
 */

/* ARGSUSED */
static void
signal_handler(int signal)
{
#ifdef DEBUG
	psignal(signal, "Thread aborting");
#endif /* DEBUG */
	if (signal == SIGUSR1) {
		pthread_exit(NULL);
	} else if (signal == SIGINT) {
		psignal(signal, "\nAborting ...");
#if defined(UPERF_SOLARIS)
		sigsend(P_PGID, getpgid(getpid()), SIGKILL);
#endif
		exit(2);
	} else {
		/* No need to do anything, the syscall will return EINTR */
	}
}

int
setup_strand_signal()
{
	struct sigaction act;
	static sigset_t sigmask;

	/* Block everything except for SIGUSR2 */
	(void) sigfillset(&sigmask);
	(void) sigdelset(&sigmask, SIGUSR2);
	(void) sigdelset(&sigmask, SIGUSR1);
	if (sigprocmask(SIG_SETMASK, &sigmask, NULL) != 0) {
		perror("sigprocmask:");
	}

	(void) memset(&act, 0, sizeof (struct sigaction));
	act.sa_handler = signal_handler;
	act.sa_mask = sigmask;

	if (sigaction(SIGUSR1, &act, NULL) != 0) {
		perror("Sigusr1");
		return (UPERF_FAILURE);
	}
	if (sigaction(SIGUSR2, &act, NULL) != 0) {
		perror("SIGUSR2");
		return (UPERF_FAILURE);
	}

	return (UPERF_SUCCESS);
}

int
master_setup_signal_handler()
{
	struct sigaction act;
	static sigset_t sigmask;

	/* Unblock everything except for SIGUSR1 */
	(void) sigemptyset(&sigmask);
	(void) sigaddset(&sigmask, SIGUSR2);
	if (sigprocmask(SIG_BLOCK, &sigmask, NULL) != 0) {
		perror("sigprocmask:");
	}
	(void) sigfillset(&sigmask);
	(void) sigdelset(&sigmask, SIGUSR1);

	(void) memset(&act, 0, sizeof (struct sigaction));
	act.sa_handler = signal_handler;
	act.sa_mask = sigmask;

	if (sigaction(SIGINT, &act, NULL) != 0) {
		perror("Sigint");
		return (UPERF_FAILURE);
	}
	if (sigaction(SIGUSR2, &act, NULL) != 0) {
		perror("SIGUSR2");
		return (UPERF_FAILURE);
	}
	if (sigaction(SIGPIPE, &act, NULL) != 0) {
		perror("SIGPIPE");
		return (UPERF_FAILURE);
	}

	return (UPERF_SUCCESS);
}
