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

#ifndef _PROTOCOL_H
#define	_PROTOCOL_H

#include "uperf.h"

#define	NUM_PROTOCOLS	8
#define	ANY_PORT		0

/*
 * Most protocol can be simplified to represent some basic, common
 * flowops. Using common flowops allows us to de-couple the protocol
 * implementation and the benchmark harness.
 *
 * This file defines the most common flowops used by protocols.
 */
typedef struct _protocol_t protocol_t;
struct _protocol_t {

	/*
	 * These functions return 0 on success, and -1 on failures.
	 * errno has the reason for failure, if any
	 */
	int (*connect) (protocol_t *, void *options);
	int (*disconnect) (protocol_t *);
	/* Returns port number on success; -1 on failure */
	int (*listen) (protocol_t *, void *);

	/* Success: new protocol object; Failure:null, errno has error */
	protocol_t *(*accept) (protocol_t *, void *options);

	/*
	 * read and write functions return no of bytes read on success.
	 * On failure, they return -1, and errno contains the reason
	 */
	int (*read) (protocol_t *, void *buf, int size, void *options);
	int (*write) (protocol_t *, void *buf, int size, void *options);
	int (*send) (protocol_t *, void *buf, int size, void *options);
	int (*recv) (protocol_t *, void *buf, int size, void *options);

	int (*wait) (protocol_t *, void *options);
	int (*getfd) (protocol_t *);
	int type;			/* Protocol Type */
	int port;			/* Remote port */
	int p_id;			/* connection ID */
	int fd;				/* connection desc */
	char host[MAXHOSTNAME];		/* Remote host */
	protocol_t *next;
	protocol_t *prev;
	void *_protocol_p;		/* Pointer to private data */
};

typedef enum {
	PROTOCOL_TCP  = 1,
	PROTOCOL_UDAPL,
	PROTOCOL_UDP,
	PROTOCOL_RDS,
	PROTOCOL_SSL,
	PROTOCOL_SCTP,
	PROTOCOL_UNSUPPORTED,
} proto_type_t;

int protocol_init(void *);
int valid_protocol(proto_type_t );
void destroy_protocol(proto_type_t, protocol_t *);
const char *protocol_to_str(proto_type_t);
proto_type_t protocol_type(char *);
protocol_t *create_protocol(proto_type_t, char *, int, int);

#endif /* _PROTOCOL_H */
