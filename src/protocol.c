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

#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "uperf.h"
#include "protocol.h"

#ifdef HAVE_SSL
#include "ssl.h"
#endif /* HAVE_SSL */

#include "generic.h"
#include "logging.h"

protocol_t *protocol_tcp_init(char *host, int port);
protocol_t *protocol_udp_init(char *rhost, int port);
void protocol_udp_fini(protocol_t *p);
void protocol_rds_fini(protocol_t *p);

int tcp_init(void *);
int udp_init(void *);
int ssl_init(void *);
int sctp_init(void *);
protocol_t *protocol_tcp_create(char *, int);
protocol_t *protocol_udp_create(char *, int);
protocol_t *protocol_ssl_create(char *, int);
protocol_t *protocol_sctp_create(char *, int);
protocol_t *protocol_rds_create(char *, int);

void generic_fini(protocol_t *);
void udp_fini(protocol_t *);
void rds_fini(protocol_t *);
void ssl_fini(protocol_t *);

typedef int (*init_func)(void *);
typedef protocol_t * (*create_func)(char *, int);
typedef void (*destroy_func)(protocol_t *);

typedef struct pdetails {
	char protocol[16];
	proto_type_t type;
	init_func init;
	create_func create;
	destroy_func destroy;
}proto_list_t;

static proto_list_t plist[] = {
	{ "tcp", PROTOCOL_TCP, NULL, protocol_tcp_create, generic_fini},
	{ "udp", PROTOCOL_UDP, NULL, protocol_udp_create, udp_fini },
#ifdef HAVE_RDS
	{ "rds", PROTOCOL_RDS, NULL, protocol_rds_create, rds_fini },
#endif /* HAVE_RDS */
#ifdef HAVE_SCTP
	{ "sctp", PROTOCOL_SCTP, NULL, protocol_sctp_create, generic_fini},
#endif /* HAVE_SCTP */
#ifdef HAVE_SSL
	{ "ssl", PROTOCOL_SSL, ssl_init, protocol_ssl_create, ssl_fini},
#endif /* HAVE_SSL */
};

static proto_list_t *
find_protocol_by_name(char *name)
{
	int i;
	for (i = 0; i < sizeof (plist)/ sizeof (proto_list_t); i++) {
		if (strcasecmp(name, plist[i].protocol) == 0)
			return (&plist[i]);
	}

	return (0);
}

static proto_list_t *
find_protocol_by_type(proto_type_t type)
{
	int i;
	for (i = 0; i < sizeof (plist)/ sizeof (proto_list_t); i++) {
		if (type == plist[i].type)
			return (&plist[i]);
	}

	return (0);
}

int
protocol_init(void *arg)
{
	int i;
	for (i = 0; i < sizeof (plist)/ sizeof (proto_list_t); i++) {
		if (plist[i].init) {
			if (plist[i].init(arg) == UPERF_FAILURE)
				return (UPERF_FAILURE);
		}
	}
	return (UPERF_SUCCESS);
}

protocol_t *
create_protocol(proto_type_t type, char *host, int port, int run_type)
{
	proto_list_t *pl = find_protocol_by_type(type);

	ASSERT(pl);
	ASSERT(pl->create);
	return (pl->create(host, port));
}

void
destroy_protocol(proto_type_t type, protocol_t *p)
{
	proto_list_t *pl = find_protocol_by_type(type);
	ASSERT(type > 0);
	ASSERT(pl);
	ASSERT(pl->destroy);
	pl->destroy(p);
}

proto_type_t
protocol_type(char *protocol)
{
	proto_list_t *pl = find_protocol_by_name(protocol);

	return (pl ? pl->type : PROTOCOL_UNSUPPORTED);
}

int
valid_protocol(proto_type_t type)
{
	proto_list_t *pl = find_protocol_by_type(type);

	return (pl ? 1 : -1);
}

const char *
protocol_to_str(proto_type_t t)
{
	switch (t) {
		case PROTOCOL_TCP: return "TCP";
		case PROTOCOL_UDAPL: return "uDAPL";
		case PROTOCOL_UDP: return "UDP";
		case PROTOCOL_SSL: return "SSL";
		case PROTOCOL_SCTP: return "SCTP";
		default: return "Unknown";
	}
}
