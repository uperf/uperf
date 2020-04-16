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

#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#include "uperf.h"
#include "logging.h"
#include "numbers.h"
#include "main.h"
#include "flowops.h"
#include "workorder.h"
#include "delay.h"
#ifdef USE_CPC
#include "hwcounter.h"
#endif /* USE_CPC */
#ifdef ENABLE_NETSTAT
#include "netstat.h"
#endif /* ENABLE_NETSTAT */

#ifdef UPERF_LINUX1
_syscall0(pid_t, gettid)
#endif /* UPERF_LINUX */

#ifdef UPERF_ANDROID
#include <hardware_legacy/power.h>
#endif /* UPERF_ANDROID */

options_t options;
static options_t *init_options(int argc, char **argv);
extern workorder_t *parse_app_profile(char *app_profile_name);

extern int slave();
extern int master(workorder_t *w);

static void
uperf_usage(char *prog)
{
	(void) printf("Uperf Version %s\n", UPERF_VERSION);
	(void) printf("Usage:   %s [-m profile] [-hvV] [-ngtTfkpaeE:X:i:P:R]\n",
	    prog);
	(void) printf("\t %s [-s] [-hvV]\n\n", prog);
	(void) printf(
	"\t-m <profile>\t Run uperf with this profile\n"
	"\t-s\t\t Slave\n"
	"\t-n\t\t No statistics\n"
	"\t-T\t\t Print Thread statistics\n"
	"\t-t\t\t Print Transaction averages\n"
	"\t-f\t\t Print Flowop averages\n"
	"\t-g\t\t Print Group statistics\n"
	"\t-k\t\t Collect kstat statistics\n"
	"\t-p\t\t Collect CPU utilization for flowops [-f assumed]\n"
	"\t-e\t\t Collect default CPU counters for flowops [-f assumed]\n"
	"\t-E <ev1,ev2>\t Collect CPU counters for flowops [-f assumed]\n"
	"\t-a\t\t Collect all statistics\n"
	"\t-X <file>\t Collect response times\n"
	"\t-i <interval>\t Collect throughput every <interval>\n"
	"\t-P <port>\t Set the master port (defaults to 20000)\n"
	"\t-R\t\t Emit raw (not transformed), time-stamped (ms) statistics\n"
	"\t-v\t\t Verbose\n"
	"\t-V\t\t Version\n"
	"\t-h\t\t Print usage\n"
	"\nMore information at http://www.uperf.org\n");
}

static char *proto[] = {
	"Supported protocols: TCP, UDP",
#ifdef HAVE_RDS
	", RDS",
#endif
#ifdef HAVE_SSL
	", SSL",
#endif
#ifdef HAVE_SCTP
	", SCTP",
#endif
	NULL
};

static void
uperf_version()
{
	int i;

#ifdef UPERF_SLAVE_ONLY
	(void) printf("Uperf_slave Version %s\n", UPERF_VERSION);
#else
	(void) printf("Uperf Version %s\n", UPERF_VERSION);
#endif /* UPERF_SLAVE_ONLY */

	(void) printf("Copyright (C) 2008 Sun Microsystems\n");
	(void) printf(
	    "License: GNU GPL Version 3 http://gnu.org/licenses/gpl.html\n");
	for (i = 0; proto[i] != NULL; i++) {
		(void) printf("%s", proto[i]);
	}
	(void) printf("\n");
#ifdef CONFIGURED_ON
	(void) printf("Configured on %s\n", CONFIGURED_ON);
#endif
#ifdef BUILD_DATE
	(void) printf("Built on %s\n", BUILD_DATE);
#endif
	(void) printf("\nReport bugs to %s\n", UPERF_EMAIL_ALIAS);
}

static options_t *
init_options(int argc, char **argv)
{
	int oserver, oclient, ofile;
	int ch;

	if (argc < 2) {
		uperf_usage(argv[0]);
		exit(1);
	}

	uperf_set_log_level(UPERF_NONVERBOSE);
	options.copt = 0;

	options.copt |= ERROR_STATS;
	options.copt |= PACKET_STATS;

	options.interval = 1000;	/* Collect throughput every 1second */
	options.master_port = MASTER_PORT;
	oserver = oclient = ofile = 0;

	while ((ch = getopt(argc, argv, "E:epTgtfknasm:X:i:P:RvVh")) != EOF) {
		switch (ch) {
#ifdef USE_CPC
		case 'E':
			if (optarg) {
				char *ev1, *ev2;
				ev1 = strtok(optarg, ",");
				ev2 = strtok(NULL, ",");
				if ((ev1 == NULL) || (ev2 == NULL)) {
					uperf_info("Unknown Event %s\n",
						optarg);
					uperf_usage(argv[0]);
					exit(1);
					break;
				} else {
					int f;
					options.ev1 = ev1;
					options.ev2 = ev2;
					f = hwcounter_validate_events(ev1, ev2);
					if (f != 0) {
						uperf_error(
					"Unknown events %s, %s\n",
						    ev1, ev2);
					    uperf_usage(argv[0]);
					    exit(1);
					} else {
					options.copt |= CPUCOUNTER_STATS;
					options.copt |= FLOWOP_STATS;
					}
					break;
				}
			}
			break;
		case 'e':
			options.copt |= CPUCOUNTER_STATS;
			options.copt |= FLOWOP_STATS;
			break;
#endif /* USE_CPC */
		case 'p':
			options.copt |= UTILIZATION_STATS;
			options.copt |= FLOWOP_STATS;
			break;
		case 'T':
			options.copt |= THREAD_STATS;
			break;
		case 'g':
			options.copt |= GROUP_STATS;
			break;
		case 't':
			options.copt |= TXN_STATS;
			break;
		case 'f':
			options.copt |= FLOWOP_STATS;
			break;
		case 'k':
			options.copt |= PACKET_STATS;
			break;
		case 'n':
			options.copt = 0;
			options.copt |= NO_STATS;
			break;
		case 'a': /* Print all */
			options.copt |= FLOWOP_STATS;
			options.copt |= TXN_STATS;
			options.copt |= PACKET_STATS;
			options.copt |= THREAD_STATS;
			options.copt |= ERROR_STATS;
			options.copt |= GROUP_STATS;
			break;
		case 's':
			options.run_choice |= UPERF_SLAVE;
			oserver++;
			break;
#ifndef UPERF_SLAVE_ONLY
		case 'm':
			options.run_choice |= UPERF_MASTER;
			oclient++;
			(void) strlcpy(options.app_profile_name, optarg,
				PATH_MAX);
			ofile++;
			break;
#endif
		case 'X':
			options.copt |= HISTORY_STATS;
			if (optarg) {
				(void) strlcpy(options.xfile, optarg,
					sizeof (options.xfile));
				options.history_fd = fopen(optarg, "w");
				if (options.history_fd == NULL)
					uperf_fatal("Cannot open file\n");
			} else {
				uperf_fatal("Please specify file \n");
			}
			break;
		case 'i':
			if (optarg) {
				options.interval = (uint64_t)
					string_to_nsec(optarg)/1.0e+6;
				if (options.interval == 0)
					uperf_fatal("Incorrect interval: %s\n",
					    optarg);
			} else {
				uperf_fatal("Please specify interval\n");
			}
			break;
		case 'P':
			if (optarg) {
				options.master_port = (int)
					string_to_int(optarg);
			}
			break;
		case 'R':
			options.copt |= RAW_STATS;
			break;
		case 'v':
			uperf_set_log_level(UPERF_VERBOSE);
			break;
		case 'V':
			uperf_version();
			exit(0);
			break;
		case 'h':
			uperf_usage(argv[0]);
			exit(0);
			break;
		case '?':
			uperf_info("Unrecognized option: -%c\n", optopt);
			uperf_usage(argv[0]);
			exit(0);
			break;
		}
	}

	/* check options */
	if (oserver && oclient) {
		(void) uperf_error("Can only be server or client. Not both!\n");
		return (NULL);
	}
	if (!oserver && !oclient) {
		(void) uperf_error("Please specify server or client.\n");
		return (NULL);
	}
	if (oclient && !ofile) {
		uperf_error("Please specify profile for client.\n");
		return (NULL);
	}
#ifdef ENABLE_NETSTAT
	if (oclient && ENABLED_PACKET_STATS(options)) {
		if (netstat_init() != 0) {
			uperf_error("Will not collect packet statistics\n");
			options.copt &= ~PACKET_STATS;
		}
	}
#endif /* ENABLE_NETSTAT */
	return (&options);
}

/* Main function - Program starting point */
int
main(int argc, char **argv)
{
	options_t *options_p;
	struct rlimit rl = {32*1024, 32*1024};
	workorder_t *worklist = NULL;

	options_p = init_options(argc, argv);
	if (options_p == NULL) {
		uperf_usage(argv[0]);
		exit(1);
	}

	if (IS_MASTER(options)) {
		worklist = parse_app_profile(options.app_profile_name);
		if (worklist == NULL) {
			uperf_error("Error parsing %s\n",
			    options.app_profile_name);
			return (1);
		}
	}

#ifdef USE_CPC
	if (ENABLED_CPUCOUNTER_STATS(options))
		hwcounter_init();
#endif
	/* Bump up our descriptor level */
	if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
		uperf_warn("Unable to increase file descriptors: %s\n",
		    strerror(errno));
#ifdef UPERF_SOLARIS
		uperf_warn("You can use\n");
		uperf_warn("prctl  -n process.max-file-descriptor -r -v <no> -i process $$\n");
		uperf_warn("to bump number of file descriptors on Solaris 10+\n");
#endif
	}

#ifdef UPERF_ANDROID
	if (init_android_alarm() == -1) {
		uperf_error("Error during Android alarm initialization: %s\n", strerror(errno));
		return (1);
	}
#endif /* UPERF_ANDROID */

	if (IS_SLAVE(options))
		return (slave());
#ifndef UPERF_SLAVE_ONLY
	else {
		int ret;
#ifdef UPERF_ANDROID
		acquire_wake_lock(PARTIAL_WAKE_LOCK, UPERF_WAKE_LOCK);
		ret = master(worklist);
		release_wake_lock(UPERF_WAKE_LOCK);
#else
		ret = master(worklist);
#endif /* UPERF_ANDROID */
		return ret;
	}
#endif
}
