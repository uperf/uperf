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

#ifndef 	_UPERF_H
#define		_UPERF_H

/* Keep the data version as 0.2.5 to avoid the version mismatch problem. */
#define UPERF_DATA_VERSION	"0.3.1"
#define	UPERF_VERSION 		"1.0.7"
#define	UPERF_VERSION_LEN 	16
#define	UPERF_EMAIL_ALIAS	"uperf-discuss@lists.sourceforge.net"

#define	MAXHOSTNAME		256
#define	PATHMAX			128
#define	MASTER_PORT		20000

#define	MAXTHREADGROUPS		1024
#define	MAXSLAVES		1024

#define UPERF_SUCCESS 		0
#define UPERF_FAILURE 		-1
#define	UPERF_DURATION_EXPIRED	4

#define	UPERF_CANFAIL		-100
#define	UPERF_TIMEOUT		-200

#define	UPERF_ANY_CONNECTION	0

/* How many times to fail before aborting? */
#define	MAX_REPEATED_FAILURES	2

#define MAXFILENAME	128
#define	MAXLINE		256

#define	USE_RWLOCKS

#define	ASSERT	assert

#define	REPEATED_SIGNAL_RETRIES	5

typedef enum {
	MASTER = 0,
	SLAVE
}role_t;

#define	STRAND_LEADER			(1<<0)
#define	STRAND_TYPE_PROCESS		(1<<1)
#define	STRAND_TYPE_THREAD		(1<<2)
#define	STRAND_ROLE_MASTER		(1<<3)
#define	STRAND_ROLE_SLAVE		(1<<4)

#define	STRAND_IS_LEADER(s)	((s)->strand_flag & STRAND_LEADER)
#define	STRAND_IS_PROCESS(s)	((s)->strand_flag & STRAND_TYPE_PROCESS)
#define	STRAND_IS_THREAD(s)	((s)->strand_flag & STRAND_TYPE_THREAD)
#define	STRAND_IS_MASTER(s)	((s)->strand_flag & STRAND_ROLE_MASTER)
#define	STRAND_IS_SLAVE(s)	((s)->strand_flag & STRAND_ROLE_SLAVE)

#define	UPERF_OK		"OK"	
#define	UPERF_NOT_OK		"Not OK"	
#define	UPERF_MAGIC		"0xbadcafedeadbeef"

#define	STAT_FLOWOP		1
#define	STAT_TXN		2
#define	STAT_THR		3
#define	STAT_ALL		4

#define	SNAP_BEGIN		1
#define	SNAP_END		2

#define	UPERF_ENDIAN_VALUE	0xBADC
#define	MAX_REMOTE_HOSTS	MAXTHREADGROUPS
#define	DEFAULT_BUFFER_SIZE	8192

#define	IS_PROCESS(a) (a->worklist->thread_type == TYPE_PROCESS) 
#define	IS_THREAD(a) (a->worklist->thread_type != TYPE_PROCESS) 

#ifndef HAVE_BSWAP
/* solaris 9 does not have this */
/*
 * Macros to reverse byte order
 */
#define BSWAP_8(x)      ((x) & 0xff)
#define BSWAP_16(x)     ((BSWAP_8(x) << 8) | BSWAP_8((x) >> 8))
#define BSWAP_32(x)     ((BSWAP_16(x) << 16) | BSWAP_16((x) >> 16))
#define BSWAP_64(x)     ((BSWAP_32(x) << 32) | BSWAP_32((x) >> 32))

#define BMASK_8(x)      ((x) & 0xff)
#define BMASK_16(x)     ((x) & 0xffff)
#define BMASK_32(x)     ((x) & 0xffffffff)
#define BMASK_64(x)     (x)
#endif /* HAVE_BSWAP */

#define   MIN(x, y)       ((x) < (y) ? (x) : (y))
#define   MAX(x, y)       ((x) > (y) ? (x) : (y))

#define	UPERF_STRAND_WAIT 		1
#define	UPERF_STRAND_INIT		2
#define	UPERF_STRAND_RUNNING		3
#define	UPERF_STRAND_FINISHED		4
#define	UPERF_STRAND_ERROR		5
#define	UPERF_STRAND_BARRIER_BEGIN	6
#define	UPERF_STRAND_BARRIER_END	7

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */

#ifndef HAVE_HRTIME_T
#define	hrtime_t	uint64_t
#endif /* HAVE_HRTIME_T */

#ifdef HAVE_PTHREAD_SELF
#define	MY_THREAD_ID	pthread_self
#elif HAVE__LWP_SELF
#define	MY_THREAD_ID	_lwp_self
#else
#error "Cannot get pthread_self nor _lwp_self"
#endif

#ifdef HAVE_LINUX_UNISTD_H
#include <linux/unistd.h>
#endif /* HAVE_LINUX_UNISTD_H */

#ifdef HAVE_SYS_TYPES_H_
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H_ */

#ifndef HAVE_STRLCPY
/* Some OS (like linux) do not have strl* routines. We fallback to
 * strn* functions
 */
#ifdef HAVE_STRNCPY
#define	strlcpy		strncpy
#define	strlcat(_dst,_src,_size)	strncat(_dst,_src,_size - strlen(_dst) - 1)
#else
#error "Cannot find safe string manipulation functions (strl* nor strn*)"
#endif /* HAVE_STRNCPY */
#endif /* HAVE_STRLCPY */

#ifdef HAVE_GETHRTIME
#define	GETHRTIME	gethrtime
#else
hrtime_t GETHRTIME();
#endif /* HAVE_GETHRTIME */

/* Forward Declarations */
typedef struct uperf_shm uperf_shm_t;
typedef struct flowop_options flowop_options_t;
typedef struct flowop flowop_t;
typedef struct transaction txn_t;
typedef struct thg group_t;
typedef struct uperf_strand strand_t;

#define UPERF_WAKE_LOCK "Uperf"

#endif	/* UPERF_H */
