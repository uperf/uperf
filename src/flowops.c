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
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>	/* for strcasecmp */
#endif

#include "flowops.h"

struct flowop_id {
	const char *name;
	uint32_t id;
};

/* Table that holds the name to type mapping */
static struct flowop_id flowops[] = {
	{ "error", 	FLOWOP_ERROR },
	{ "Read", 	FLOWOP_READ },
	{ "Write", 	FLOWOP_WRITE },
	{ "Connect", 	FLOWOP_CONNECT },
	{ "Disconnect", FLOWOP_DISCONNECT },
	{ "Accept", 	FLOWOP_ACCEPT },
	{ "Nop", 	FLOWOP_NOP },
	{ "Think", 	FLOWOP_THINK },
	{ "send", 	FLOWOP_SEND },
	{ "recv", 	FLOWOP_RECV },
	{ "sendfile", 	FLOWOP_SENDFILE },
	{ "sendfilev", 	FLOWOP_SENDFILEV },
};

struct flowop_opp {
	flowop_type_t fop;
	flowop_type_t opposite_fop;
};

static struct flowop_opp opp_flowopsp[] = {
	{ FLOWOP_READ,		FLOWOP_WRITE },
	{ FLOWOP_SEND,		FLOWOP_RECV },
	{ FLOWOP_ACCEPT,	FLOWOP_CONNECT},
	{ FLOWOP_SENDFILE,	FLOWOP_READ},
	{ FLOWOP_SENDFILEV,	FLOWOP_READ},
};

flowop_type_t
flowop_opposite(flowop_type_t type)
{
	int i;

	for (i = 0; i < sizeof (opp_flowopsp)/sizeof (struct flowop_opp); i++) {
		if (type == opp_flowopsp[i].fop)
			return (opp_flowopsp[i].opposite_fop);
		else if (type == opp_flowopsp[i].opposite_fop)
			return (opp_flowopsp[i].fop);
	}

	return (type);
}

/*
 * Flowop string to type
 */
flowop_type_t
flowop_type(char *flowstr)
{
	int i;

	for (i = 0; i < FLOWOP_NUMTYPES; i++) {
		if (strcasecmp(flowstr, flowops[i].name) == 0)
			return (flowops[i].id);
	}

	return (FLOWOP_ERROR);
}
