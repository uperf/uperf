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

#ifndef _PARSE_H
#define	_PARSE_H

#define	TOKEN_PROFILE_START	0
#define	TOKEN_PROFILE_END	1
#define	TOKEN_GROUP_START	2
#define	TOKEN_GROUP_END		3
#define	TOKEN_TXN_START		4
#define	TOKEN_TXN_END		5
#define	TOKEN_FLOWOP_START	6
#define	TOKEN_XML_END		7
#define	TOKEN_NAME		8
#define	TOKEN_ITERATIONS	9
#define	TOKEN_TYPE		10	
#define	TOKEN_OPTIONS		11	
#define	TOKEN_DURATION		12
#define	TOKEN_NTHREADS		14
#define	TOKEN_NPROCESSES	15
#define	TOKEN_RATE		16
#define	TOKEN_ERROR		99

struct symbol {
	char *symbol;
	int type;
	struct symbol *next;
	struct symbol *prev;
};


#endif /* _PARSE_H */
