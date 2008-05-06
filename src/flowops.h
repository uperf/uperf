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

#ifndef _FLOWOP_H
#define _FLOWOP_H

typedef enum {
	FLOWOP_ERROR = 0,
	FLOWOP_READ,
	FLOWOP_WRITE,
	FLOWOP_CONNECT,
	FLOWOP_DISCONNECT,
	FLOWOP_ACCEPT,
	FLOWOP_NOP,
	FLOWOP_THINK,
	FLOWOP_SEND,
	FLOWOP_RECV,
	FLOWOP_SENDFILEV,
	FLOWOP_SENDFILE,
	FLOWOP_NUMTYPES
}flowop_type_t;

typedef enum {
	THINK_IDLE = 1,
	THINK_BUSY
}think_type_t;

flowop_type_t flowop_type(char *);
flowop_type_t flowop_opposite(flowop_type_t);

#endif /* _FLOWOP_H */
