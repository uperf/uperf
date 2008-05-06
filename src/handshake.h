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

#ifndef _HANDSHAKE_H
#define _HANDSHAKE_H

#define	HANDSHAKE_PHASE_1	1
#define	HANDSHAKE_PHASE_2	2
#define	HANDSHAKE_PHASE_3	3

typedef struct hs_phase1 {
	char magic[32]; /* UPERF_MAGIC */
	uint32_t endian;
	uint32_t phase;
	uint32_t no_slave_info;
	uint32_t padding;
	uint32_t protocol[NUM_PROTOCOLS];
	char version[UPERF_VERSION_LEN];
	char host[MAXHOSTNAME];
}hs_phase1_t;

typedef struct hs_phase1_ack {
	char status[64];
	char message[128];
}hs_phase1_ack_t;


typedef struct hs_phase2 {
	hs_phase1_t p1;
}hs_phase2_t;
typedef struct hs_phase2_ack {
	hs_phase1_ack_t p1a;
	uint32_t no_slave_info;
}hs_phase2_ack_t;

int slave_handshake_p2_success(slave_info_t *, int, protocol_t *, int);
int slave_handshake_p2_failure(char *, protocol_t *, int);
int handshake(uperf_shm_t *, workorder_t *);
int slave_handshake(uperf_shm_t *, protocol_t *);
int slave_handshake_p2_complete(uperf_shm_t *, protocol_t *);
int handshake_end_master(uperf_shm_t *, workorder_t *);
int handshake_end_slave(slave_info_t *, int, protocol_t *, int);

#endif /* _HANDSHAKE_H */
