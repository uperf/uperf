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

#ifndef _GOODBYE_H
#define	_GOODBYE_H

typedef struct {
	uint64_t	elapsed_time;
	uint64_t	error;			/* Errors */
	uint64_t	bytes_xfer;
	uint64_t	count;			/* Execute count */
}goodbye_stat_t;

/* What types of messages do we need to exchange at end? 
 * 2. Error opening connection...
 * 3. Warning: Could not set window size
 * 4. Successful completion
 * 5. slave  19.56MB/6.05(s) =   27.11Mb/s (3.00% errors)
 */

#define	MESSAGE_ERROR	0xaa
#define	MESSAGE_WARNING	0xbb
#define	MESSAGE_INFO	0xcc
#define	MESSAGE_NONE	0xdd

#define	GOODBYE_MESSAGE_LEN 	512
#define GOODBYE_MAGIC		"So Long, and Thanks for All the Fish"

typedef struct {
	char		magic[64];
	uint64_t 	msg_type;
	goodbye_stat_t	gstat;
	char 		message[GOODBYE_MESSAGE_LEN];
}goodbye_t;

int send_goodbye(goodbye_t *, protocol_t *);
int recv_goodbye(goodbye_t *, protocol_t *, int);
int bitswap_goodbye_t(goodbye_t *);

#endif /* _GOODBYE_H */
