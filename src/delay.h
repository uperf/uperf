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

#ifndef _DELAY_H
#define _DELAY_H

/*
 * Delay functions:
 * Two kinds of delay functions viz sleep and busy spin. Duration
 * is specified in nanoseconds (No guarantees made as dependent on
 * underlying syscall)
 * Return 0 on success, else errno
 */
int uperf_sleep(hrtime_t);
int uperf_spin(hrtime_t);

#ifdef UPERF_ANDROID
/*
 * Initialize alarm to be able to wake up the device between transactions / flowops
 */
int init_android_alarm();
#endif /* UPERF_ANDROID */

#endif /* _DELAY_H */
