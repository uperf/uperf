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

#ifndef _GENERIC_H
#define	_GENERIC_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int name_to_addr(const char *, struct sockaddr_storage *);
int generic_socket(protocol_t *, int, int);
int generic_connect(protocol_t *, struct sockaddr_storage *);
int generic_listen(protocol_t *, int, void *);
int generic_undefined(protocol_t *, void *);
int generic_write(protocol_t *, void *, int, void *);
int generic_disconnect(protocol_t *);
int generic_read(protocol_t *, void *, int, void *);
int generic_accept(protocol_t *, protocol_t *, void *);
void generic_fini(protocol_t *);
int generic_set_socket_buffer(int, int);
int generic_verify_socket_buffer(int, int);
int generic_setfd_nonblock(int);
int generic_poll(int, int, short);
void set_tcp_options(int fd, flowop_options_t *f);
int generic_recv(protocol_t *p, void *buffer, int size, void *options);
int generic_send(protocol_t *p, void *buffer, int size, void *options);
#endif /* _GENERIC_H */
