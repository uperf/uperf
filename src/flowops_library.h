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

#ifndef FLOWOPS_LIBARARY_H
#define FLOWOPS_LIBARARY_H

int flowop_think(strand_t *, flowop_t *);
int flowop_read(strand_t *, flowop_t *);
int flowop_write(strand_t *, flowop_t *);
int flowop_unknown(strand_t *, flowop_t *);
int flowop_accept(strand_t *, flowop_t *);
int flowop_disconnect(strand_t *, flowop_t *);
int flowop_connect(strand_t *, flowop_t *);
execute_func flowop_get_execute_func(int);

#endif /* FLOWOPS_LIBARARY_H */
