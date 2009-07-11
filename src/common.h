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

#ifndef _COMMON_H
#define	_COMMON_H

#define	UPERF_COMMAND_MAGIC	"uperf_command"
#define	ALL_GROUPS		-1

typedef enum {
	UPERF_CMD_NEXT_TXN,
	UPERF_CMD_ABORT,
	UPERF_CMD_SEND_STATS,
	UPERF_CMD_ERROR
} uperf_cmd;

typedef struct {
	char 		magic[64];
	uperf_cmd	command;
	uint32_t 	value;
} uperf_command_t;

int preprocess_accepts(uperf_shm_t *, group_t *, slave_info_t **, int );
int update_strand_with_slave_info(uperf_shm_t *, slave_info_t *, char *, int, int);
int uperf_get_command(protocol_t *, uperf_command_t*, int);
int uperf_send_command(protocol_t *p, uperf_cmd command, uint32_t val);
int ensure_write(protocol_t *p, void *buffer, int size);
int ensure_read(protocol_t *p, void *buffer, int size);

#endif /* _COMMON_H */
