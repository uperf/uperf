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
#define UPERF_DATA_VERSION	"0.3"
#define	UPERF_VERSION 		"1.0.1"
#define	UPERF_VERSION_LEN 	128
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

#ifdef UPERF_LINUX

#include <sys/types.h>
#include <linux/unistd.h>
#include <stdint.h>

#define	hrtime_t	uint64_t
#define	_lwp_self	gettid
#define	strlcpy		strncpy
#define	strlcat		strncat

hrtime_t GETHRTIME();
#endif /* UPERF_LINUX */

#ifdef UPERF_SOLARIS
#define GETHRTIME gethrtime

#endif /* UPERF_SOLARIS */

/* Forward Declarations */
typedef struct uperf_shm uperf_shm_t;
typedef struct flowop_options flowop_options_t;
typedef struct flowop flowop_t;
typedef struct transaction txn_t;
typedef struct thg group_t;
typedef struct uperf_strand strand_t;

#endif	/* UPERF_H */
