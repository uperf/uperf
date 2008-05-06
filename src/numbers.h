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

#ifndef _NUMBERS_H
#define _NUMBERS_H

int string_to_int(char *);
uint64_t string_to_nsec(char *);
void print_decimal(double value, int size, int is_bits);
void adaptive_print_time(double value, int width);

#define	PRINT_NUM(v, s)		print_decimal((v), (s), 0)
#define	PRINT_NUMb(v, s)	print_decimal((v), (s), 1)
#define	PRINT_TIME(v, s)	adaptive_print_time((v), (s))

#endif /* _NUMBERS_H */
