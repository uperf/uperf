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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <math.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#ifdef HAVE_SYS_INT_LIMITS_H
#include <sys/int_limits.h>
#endif				/* HAVE_SYS_INT_LIMITS_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#include "logging.h"

static int
str2shift(const char *buf)
{
	const char *ends = "BKMGTPEZ";
	int i;
	if (buf[0] == '\0')
		return (0);
	for (i = 0; i < strlen(ends); i++) {
		if (toupper(buf[0]) == ends[i])
			break;
	}
	if (i == strlen(ends)) {
		uperf_info("invalid numeric suffix :%s\n", buf);
		return (-1);
	}
	return (10 * i);
}
/*
 * Convert a human readable string to its real value. For ex, convert 64k
 * to a real number. (-1) is returned to indicate error
 */
int
string_to_int(char *value)
{
	char *end;
	int shift = 0;
	int num;
	char *tmp = value;
	if (value[0] == '$') {
		tmp = getenv(&value[0]);
		if (tmp == NULL) {
			return (-1);
		}
	}
	if ((tmp[0] < '0' || tmp[0] > '9') && tmp[0] != '.') {
		return (-1);
	}
	errno = 0;
	num = strtoll(tmp, &end, 10);
	if (errno == ERANGE) {
		uperf_info("%s is too large\n", tmp);
		return (-1);
	}
	if (*end == '.') {
		double fval = strtod(tmp, &end);
		if ((shift = str2shift(end)) == -1)
			return (-1);
		fval *= pow(2.0, (double) shift);
		if (fval > UINT32_MAX) {
			uperf_info("%s is too large\n", tmp);
			return (-1);
		}
		num = (int) fval;
	} else {
		if ((shift = str2shift(end)) == -1)
			return (-1);
		/* Check for overflow */
		if (shift > 32 || (num << shift) >> shift != num) {
			uperf_info("%s is too large\n", tmp);
			return (-1);
		}
		num <<= shift;
	}
	uperf_debug("%s = %d\n", value, num);
	return (num);
}
static float
get_multiplier(char *suffix)
{
	/* Default is seconds */
	if (suffix == 0 || !*suffix)
		return (1.0e+9);
	switch (tolower(suffix[0])) {
	case 's':
		return (1.0e+9);
	case 'm':
		return (1.0e+6);
	case 'u':
		return (1.0e+3);
	case 'n':
		return (1.0);
	default:
		uperf_error("%s not valid time suffix\n", suffix);
		return (-1.0);
	}
}
/*
 * convert number to nanoseconds. accepted input: 20s, 20, 20ms, 20ns,
 * 20us
 */
uint64_t
string_to_nsec(char *value)
{
	char *end;
	uint64_t num;
	char *tmp = value;
	if (value[0] == '$') {
		tmp = getenv(&value[0]);
		if (tmp == NULL) {
			return (-1);
		}
	}
	if ((tmp[0] < '0' || tmp[0] > '9') && tmp[0] != '.') {
		uperf_info("%s is not a valid number\n", tmp);
		return (0);
	}
	errno = 0;
	num = strtoll(tmp, &end, 10);
	if (errno == ERANGE) {
		uperf_info("%s is too large\n", tmp);
		return (0);
	}
	if (*end == '.') {
		float val = strtod(tmp, &end);
		if (errno == ERANGE) {
			uperf_info("%s is too large\n", tmp);
			return (-1);
		}
		val *= get_multiplier(end);
		return ((uint64_t) val);
	} else {
		return ((uint64_t) (num * get_multiplier(end)));
	}
}

char *
decimal_to_string(double value, char *result, int size, int is_bit)
{
	char suffix[] = " KMGTPEZ";
	int pos = 0;
	double div;

	(void) memset(result, 0, size);
	div = is_bit ? 1000.0 : 1024.0;
	if (value == 0)
		return ("0");
	while (value > div && pos < sizeof (suffix)) {
		value /= div;
		pos++;
	}
	if (is_bit == 1) {
		if (pos == 0)
			(void) snprintf(result, size, "%.2fb/s", value);
		else
			(void) snprintf(result, size, "%.2f%cb/s", value,
					suffix[pos]);
	} else {
		if (pos == 0)
			(void) snprintf(result, size, "%.2fB", value);
		else
			(void) snprintf(result, size, "%.2f%cB", value,
					suffix[pos]);
	}
	return (result);
}

void
print_decimal(double value, int size, int is_bit)
{
	char temp[128];
	(void) printf("%*s ", size, decimal_to_string(value, temp, size,
			is_bit));
}

void
adaptive_print_time(double value, int width)
{
	char str[128];
	int acutal_width;
	int index = 0;
	static char prefix[] = "num";
	char format[32];

	if (width > 127) {
		width = 127;
	}
	while ((value >= 1000.0) && (index < strlen(prefix))) {
		index++;
		value /= 1000.0;
	}
	if (index == strlen(prefix)) {
		acutal_width = width - 1;
		(void) snprintf(format, sizeof(format), "%%%d.%dfs", acutal_width, 2);
		(void) snprintf(str, width + 1, format, value);
	} else {
		acutal_width = width - 2;
		(void) snprintf(format, sizeof(format), "%%%d.%df%%cs", acutal_width, 2);
		(void) snprintf(str, width + 1, format, value, prefix[index]);
	}
	(void) printf("%*s ", width, str);
}
